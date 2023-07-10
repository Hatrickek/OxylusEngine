#include "VulkanShader.h"
#include "VulkanContext.h"

#include <filesystem>
#include <fstream>
#include <fmt/format.h>
#include <spirv_reflect.h>

#include "VulkanRenderer.h"
#include "Render/ShaderLibrary.h"
#include "Utils/Profiler.h"
#include "Utils/FileUtils.h"
#include "Utils/VulkanUtils.h"

namespace Oxylus {
  struct Includer : shaderc::CompileOptions::IncluderInterface {
    shaderc_include_result* GetInclude(const char* requested_source,
                                       shaderc_include_type type,
                                       const char* requesting_source,
                                       size_t include_depth) override {
      const std::string name = std::string(requested_source);
      const auto dir = std::filesystem::path(Application::Get()->Spec.ResourcesPath) / fmt::format("Shaders/{0}", name);
      const std::string filepath = dir.string();
      const auto content = FileUtils::ReadFile(filepath);
      OX_CORE_ASSERT(content,
        fmt::format("Couldn't load the include file: {0} for shader: {1}", requested_source, requesting_source).c_str(
        ));

      const auto container = new std::array<std::string, 2>;
      (*container)[0] = name;
      (*container)[1] = content.value();

      const auto result = new shaderc_include_result;
      result->content = (*container)[1].data();
      result->content_length = (*container)[1].size();

      result->source_name = (*container)[0].data();
      result->source_name_length = (*container)[0].size();

      result->user_data = container;

      return result;
    }

    void ReleaseInclude(shaderc_include_result* data) override {
      delete (std::array<std::string, 2>*)data->user_data;
      delete data;
    }
  };

  static std::string GetCacheDirectory() {
    return (std::filesystem::path(Application::Get()->Spec.ResourcesPath) / "Shaders/Compiled/").string();
  }

  static void CreateCacheDirectoryIfNeeded() {
    const std::string cacheDirectory = GetCacheDirectory();
    if (!std::filesystem::exists(cacheDirectory))
      std::filesystem::create_directories(cacheDirectory);
  }

  static const char* GetCompiledFileExtension(vk::ShaderStageFlagBits stage) {
    switch (stage) {
      case vk::ShaderStageFlagBits::eFragment: return "_compiled.frag";
      case vk::ShaderStageFlagBits::eVertex: return "_compiled.vert";
      case vk::ShaderStageFlagBits::eCompute: return "_compiled.comp";
      default: break;
    }
    return "";
  }

  std::filesystem::path VulkanShader::GetCachedDirectory(const vk::ShaderStageFlagBits stage,
                                                         const std::filesystem::path& cacheDirectory) {
    return cacheDirectory / m_VulkanFilePath[stage].stem()
                                                   .string()
                                                   .append(GetCompiledFileExtension(stage));
  }

  static shaderc_shader_kind GLShaderStageToShaderC(vk::ShaderStageFlagBits stage) {
    switch (stage) {
      case vk::ShaderStageFlagBits::eVertex: return shaderc_glsl_vertex_shader;
      case vk::ShaderStageFlagBits::eFragment: return shaderc_glsl_fragment_shader;
      case vk::ShaderStageFlagBits::eCompute: return shaderc_glsl_compute_shader;
      default: break;
    }
    return (shaderc_shader_kind)0;
  }

  VulkanShader::VulkanShader(const ShaderCI& shaderCI) {
    CreateCacheDirectoryIfNeeded();

    m_ShaderDesc = shaderCI;

    CreateShader();
  }

  void VulkanShader::CreateShader() {
    ProfilerTimer timer;

    shaderc::CompileOptions options;
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
    options.SetTargetSpirv(shaderc_spirv_version_1_6);
#if defined (OX_RELEASE) || defined (OX_DISTRIBUTION)
    options.SetOptimizationLevel(shaderc_optimization_level_performance);
#else
    options.SetOptimizationLevel(shaderc_optimization_level_zero);
#endif
    options.SetIncluder(std::make_unique<Includer>());

    if (!m_ShaderDesc.ComputePath.empty()) {
      m_VulkanFilePath[vk::ShaderStageFlagBits::eCompute] = m_ShaderDesc.ComputePath;
      const auto& content = FileUtils::ReadFile(m_ShaderDesc.ComputePath);
      ReadOrCompile(vk::ShaderStageFlagBits::eCompute, content.value_or(""), options);
    }
    if (!m_ShaderDesc.VertexPath.empty()) {
      m_VulkanFilePath[vk::ShaderStageFlagBits::eVertex] = m_ShaderDesc.VertexPath;
      const auto& content = FileUtils::ReadFile(m_ShaderDesc.VertexPath);
      ReadOrCompile(vk::ShaderStageFlagBits::eVertex, content.value_or(""), options);
    }
    if (!m_ShaderDesc.FragmentPath.empty()) {
      m_VulkanFilePath[vk::ShaderStageFlagBits::eFragment] = m_ShaderDesc.FragmentPath;
      const auto& content = FileUtils::ReadFile(m_ShaderDesc.FragmentPath);
      ReadOrCompile(vk::ShaderStageFlagBits::eFragment, content.value_or(""), options);
    }

    for (auto&& [stage, source] : m_VulkanSPIRV) {
      CreateShaderModule(stage, source);
    }

    if (m_ShaderDesc.Name.empty()) {
      m_ShaderDesc.Name = std::filesystem::path(m_ShaderDesc.VertexPath).stem().string();
    }

    // Reflection and layouts
    std::vector<std::vector<vk::DescriptorSetLayoutBinding>> layoutBindings{10};

    if (!m_VulkanSPIRV[vSS::eVertex].empty()) {
      auto data = GetReflectionData(m_VulkanSPIRV[vSS::eVertex]);
      CreateLayoutBindings(data, layoutBindings);
      m_ReflectionData.emplace_back(data);
    }
    if (!m_VulkanSPIRV[vSS::eFragment].empty()) {
      auto data = GetReflectionData(m_VulkanSPIRV[vSS::eFragment]);
      CreateLayoutBindings(data, layoutBindings);
      m_ReflectionData.emplace_back(data);
    }
    if (!m_VulkanSPIRV[vSS::eCompute].empty()) {
      auto data = GetReflectionData(m_VulkanSPIRV[vSS::eCompute]);
      CreateLayoutBindings(data, layoutBindings);
      m_ReflectionData.emplace_back(data);
    }

    if (layoutBindings.empty())
      layoutBindings.emplace_back().emplace_back();

    m_DescriptorSetLayouts = CreateLayout(layoutBindings);

    m_PipelineLayout = CreatePipelineLayout(m_DescriptorSetLayouts);

    // Free modules
    for (auto& module : m_ReflectModules)
      spvReflectDestroyShaderModule(&module);
    m_ReflectModules.clear();

    // Clear cache
    m_VulkanSPIRV.clear();

    timer.Print(fmt::format("Shader {0} loaded", m_ShaderDesc.Name));

    m_Loaded = true;
  }

  void VulkanShader::ReadOrCompile(vk::ShaderStageFlagBits stage,
                                   const std::string& source,
                                   const shaderc::CompileOptions& options) {
    std::filesystem::path cacheDirectory = GetCacheDirectory();
    std::filesystem::path cachedPath = GetCachedDirectory(stage, cacheDirectory);
    std::ifstream in(cachedPath, std::ios::in | std::ios::binary);
    if (in.is_open()) {
      in.seekg(0, std::ios::end);
      auto size = in.tellg();
      in.seekg(0, std::ios::beg);

      auto& data = m_VulkanSPIRV[stage];
      data.resize(size / sizeof(uint32_t));
      in.read((char*)data.data(), size);
    }
    else if (!source.empty()) {
      shaderc::Compiler compiler;
      shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source,
        GLShaderStageToShaderC(stage),
        m_VulkanFilePath[stage].string().c_str(),
        options);
      if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
        OX_CORE_ERROR(module.GetErrorMessage());
      }

      m_VulkanSPIRV[stage] = std::vector(module.cbegin(), module.cend());

      std::ofstream out(cachedPath, std::ios::out | std::ios::binary);
      if (out.is_open()) {
        auto& data = m_VulkanSPIRV[stage];
        out.write((char*)data.data(), data.size() * sizeof(uint32_t));
        out.flush();
        out.close();
      }
    }
    OX_CORE_ASSERT(!m_VulkanSPIRV[stage].empty());
  }

  void VulkanShader::CreateShaderModule(vk::ShaderStageFlagBits stage, const std::vector<unsigned>& source) {
    vk::ShaderModuleCreateInfo moduleCreateInfo;
    moduleCreateInfo.codeSize = source.size() * 4;
    moduleCreateInfo.pCode = source.data();

    const auto result = VulkanContext::Context.Device.createShaderModule(&moduleCreateInfo,
      nullptr,
      &m_ShaderModule[stage]);
    if (result != vk::Result::eSuccess) {
      OX_CORE_ERROR("Couldn't create shader module");
    }
    m_ShaderStage[stage].stage = stage;
    m_ShaderStage[stage].module = m_ShaderModule[stage];
    if (m_ShaderDesc.EntryPoint.empty()) {
      OX_CORE_ERROR("Shader can't have an empty entry point: {} ", m_VulkanFilePath[stage].string());
    }
    m_ShaderStage[stage].pName = m_ShaderDesc.EntryPoint.c_str();
  }

  VulkanShader::ReflectionData VulkanShader::GetReflectionData(const std::vector<uint32_t>& spirvBytes) {
    // Generate reflection data for a shader
    SpvReflectShaderModule module;
    SpvReflectResult result = spvReflectCreateShaderModule(spirvBytes.size() * 4, spirvBytes.data(), &module);
    OX_CORE_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);

    // Descriptor sets
    uint32_t setCount = 0;
    result = spvReflectEnumerateDescriptorSets(&module, &setCount, nullptr);
    OX_CORE_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
    std::vector<SpvReflectDescriptorSet*> descriptorSets(setCount);
    result = spvReflectEnumerateDescriptorSets(&module, &setCount, descriptorSets.data());
    OX_CORE_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);

    // Block variables
    uint32_t blockCount = 0;
    result = spvReflectEnumeratePushConstantBlocks(&module, &blockCount, nullptr);
    OX_CORE_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
    std::vector<SpvReflectBlockVariable*> blockVariables(blockCount);
    result = spvReflectEnumeratePushConstantBlocks(&module, &blockCount, blockVariables.data());
    OX_CORE_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);

    m_ReflectModules.emplace_back(module);

    ReflectionData data = {};
    for (const auto& set : descriptorSets) {
      auto& binding = data.Bindings.emplace_back(DescriptorSetData{set->set, set->binding_count});
      for (uint32_t i = 0; i < set->binding_count; i++) {
        binding.bindings.emplace_back(DescriptorBindingData{
            "",
            set->bindings[i]->binding,
            set->bindings[i]->set,
            (vk::DescriptorType)set->bindings[i]->descriptor_type,
            set->bindings[i]->count
          }
        );
      }
    }
    for (uint32_t i = 0; i < blockCount; i++)
      data.PushConstants.emplace_back(PushConstantData{
        blockVariables[i]->size,
        blockVariables[i]->members[0].offset,
        (vk::ShaderStageFlagBits)module.shader_stage
      });
    data.Stage |= (vk::ShaderStageFlagBits)module.shader_stage;

    return data;
  }

  void VulkanShader::CreateLayoutBindings(const ReflectionData& reflectionData,
                                          std::vector<std::vector<vk::DescriptorSetLayoutBinding>>& layoutBindings) {
    const auto& [data, block, stage] = reflectionData;
    for (const auto& descriptor : data) {
      for (uint32_t i = 0; i < descriptor.binding_count; i++) {
        // TODO(hatrickek): Temporary solution to not add bindings that are in both stages.
        if (!layoutBindings.empty()) {
          if (layoutBindings[descriptor.bindings[i].set].size() > descriptor.bindings[i].binding) {
            if (layoutBindings[descriptor.bindings[i].set][i].binding == descriptor.bindings[i].binding) {
              layoutBindings[descriptor.bindings[i].set][i].stageFlags |= stage;
              continue;
            }
          }
        }
        // Layout binding
        vk::DescriptorSetLayoutBinding binding;
        binding.binding = descriptor.bindings[i].binding;
        binding.descriptorCount = descriptor.bindings[i].count;
        binding.descriptorType = (vk::DescriptorType)descriptor.bindings[i].descriptor_type;
        binding.stageFlags = stage;
        layoutBindings[descriptor.bindings[i].set].emplace_back(binding);

        // WriteDescriptorSet Data
        m_WriteDescriptorSets.reserve(40);
        m_WriteDescriptorSets[descriptor.bindings[i].set].emplace_back(vk::WriteDescriptorSet{
          {},
          descriptor.bindings[i].binding,
          0,
          descriptor.bindings[i].count,
          (vk::DescriptorType)descriptor.bindings[i].descriptor_type
        });
      }
    }
  }

  std::vector<vk::DescriptorSetLayout> VulkanShader::CreateLayout(
    const std::vector<std::vector<vk::DescriptorSetLayoutBinding>>& layoutBindings) {
    std::vector<vk::DescriptorSetLayout> layouts;

    for (auto& layoutBinding : layoutBindings) {
      // Set layout
      vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;
      descriptorSetLayoutCreateInfo.bindingCount = (uint32_t)layoutBinding.size();
      descriptorSetLayoutCreateInfo.pBindings = layoutBinding.data();
      vk::DescriptorSetLayout setLayout;
      const auto result = VulkanContext::GetDevice().createDescriptorSetLayout(&descriptorSetLayoutCreateInfo,
        nullptr,
        &setLayout);
      VulkanUtils::CheckResult(result);
      layouts.emplace_back(setLayout);
    }

    return layouts;
  }

  vk::PipelineLayout VulkanShader::CreatePipelineLayout(
    const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts) const {
    vk::PipelineLayoutCreateInfo createInfo;
    createInfo.setLayoutCount = (uint32_t)descriptorSetLayouts.size();
    createInfo.pSetLayouts = descriptorSetLayouts.data();
    std::vector<vk::PushConstantRange> pushConstantRanges = {};
    for (const auto& data : m_ReflectionData)
      for (const auto& block : data.PushConstants)
        pushConstantRanges.emplace_back(data.Stage, block.Offset, block.Size);
    createInfo.pPushConstantRanges = pushConstantRanges.data();
    createInfo.pushConstantRangeCount = (uint32_t)pushConstantRanges.size();
    const auto result = VulkanContext::GetDevice().createPipelineLayout(createInfo);
    VulkanUtils::CheckResult(result.result);
    return result.value;
  }

  void VulkanShader::Unload() {
    if (!m_Loaded)
      return;
    for (const auto& [stage, _] : m_VulkanSPIRV) {
      VulkanContext::Context.Device.destroyShaderModule(m_ShaderModule[stage], nullptr);
    }
    m_ShaderStages.clear();
    m_Loaded = false;
  }

  void VulkanShader::Destroy() {
    Unload();
    ShaderLibrary::RemoveShader(GetName());
  }

  void VulkanShader::Reload() {
    VulkanRenderer::WaitDeviceIdle();
    // Delete compiled files
    for (const auto& [stage, flag] : m_ShaderStage) {
      const std::filesystem::path cacheDirectory = GetCacheDirectory();
      std::filesystem::path cachedPath = GetCachedDirectory(stage, cacheDirectory);
      std::filesystem::remove(cachedPath);
    }
    m_OnReloadBeginEvent();
    Unload();
    CreateShader();
    m_OnReloadEndEvent();
  }

  void VulkanShader::OnReloadBegin(const std::function<void()>& event) {
    m_OnReloadBeginEvent = event;
  }

  void VulkanShader::OnReloadEnd(const std::function<void()>& event) {
    m_OnReloadEndEvent = event;
  }

  std::vector<vk::PipelineShaderStageCreateInfo>& VulkanShader::GetShaderStages() {
    if (m_ShaderStages.empty()) {
      m_ShaderStages = {GetVertexShaderStage(), GetFragmentShaderStage()};
    }
    return m_ShaderStages;
  }

  std::vector<vk::WriteDescriptorSet> VulkanShader::GetWriteDescriptorSets(
    const vk::DescriptorSet& descriptorSet,
    uint32_t set) {
    std::vector<vk::WriteDescriptorSet> writeDescriptorSets = {};
    const auto& blankImage = VulkanImage::GetBlankImage();

    for (const auto& writeSet : m_WriteDescriptorSets[set]) {
      writeDescriptorSets.emplace_back(
        descriptorSet,
        writeSet.dstBinding,
        0,
        writeSet.descriptorCount,
        writeSet.descriptorType,
        &blankImage->GetDescImageInfo()
      );
    }
    return writeDescriptorSets;
  }
}
