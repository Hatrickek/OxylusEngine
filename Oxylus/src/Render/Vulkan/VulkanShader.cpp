#include "src/oxpch.h"
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
      const std::string filepath = std::string(fmt::format("Resources/Shaders/{0}", name));
      const auto content = FileUtils::ReadFile(filepath);
      OX_CORE_ASSERT(content,
        fmt::format("Couldn't load the include file: {0} for shader: {1}", requested_source, requesting_source).c_str());

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

  static const char* GetCacheDirectory() {
    return "Resources/Shaders/Compiled/";
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
    }
    return "";
  }

  std::filesystem::path VulkanShader::GetCachedDirectory(vk::ShaderStageFlagBits stage,
                                                         std::filesystem::path cacheDirectory) {
    return cacheDirectory / (m_VulkanFilePath[stage].filename().string() + GetCompiledFileExtension(stage));
  }

  static shaderc_shader_kind GLShaderStageToShaderC(vk::ShaderStageFlagBits stage) {
    switch (stage) {
      case vk::ShaderStageFlagBits::eVertex: return shaderc_glsl_vertex_shader;
      case vk::ShaderStageFlagBits::eFragment: return shaderc_glsl_fragment_shader;
      case vk::ShaderStageFlagBits::eCompute: return shaderc_glsl_compute_shader;
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
    if (!m_ShaderDesc.ComputePath.empty()) {
      m_VulkanFilePath[vk::ShaderStageFlagBits::eCompute] = m_ShaderDesc.ComputePath;
      const auto& content = FileUtils::ReadFile(m_ShaderDesc.ComputePath);
      OX_CORE_ASSERT(content, fmt::format("Couldn't load the shader file: {0}", m_ShaderDesc.ComputePath).c_str());
      m_VulkanSourceCode[vk::ShaderStageFlagBits::eCompute] = content.value();
    }
    if (!m_ShaderDesc.VertexPath.empty()) {
      m_VulkanFilePath[vk::ShaderStageFlagBits::eVertex] = m_ShaderDesc.VertexPath;
      const auto& content = FileUtils::ReadFile(m_ShaderDesc.VertexPath);
      OX_CORE_ASSERT(content, fmt::format("Couldn't load the shader file: {0}", m_ShaderDesc.ComputePath).c_str());
      m_VulkanSourceCode[vk::ShaderStageFlagBits::eVertex] = content.value();
    }
    if (!m_ShaderDesc.FragmentPath.empty()) {
      m_VulkanFilePath[vk::ShaderStageFlagBits::eFragment] = m_ShaderDesc.FragmentPath;
      const auto& content = FileUtils::ReadFile(m_ShaderDesc.FragmentPath);
      OX_CORE_ASSERT(content, fmt::format("Couldn't load the shader file: {0}", m_ShaderDesc.ComputePath).c_str());
      m_VulkanSourceCode[vk::ShaderStageFlagBits::eFragment] = content.value();
    }

    shaderc::CompileOptions options;
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
    options.SetTargetSpirv(shaderc_spirv_version_1_6);
#if defined (OX_RELEASE) || defined (OX_DIST)
    options.SetOptimizationLevel(shaderc_optimization_level_performance);
#else
    options.SetOptimizationLevel(shaderc_optimization_level_zero);
#endif
    options.SetIncluder(std::make_unique<Includer>());

    for (auto&& [stage, source] : m_VulkanSourceCode) {
      ReadOrCompile(stage, source, options);
    }
    for (auto&& [stage, source] : m_VulkanSPIRV) {
      CreateShaderModule(stage, source);
    }

    if (m_ShaderDesc.Name.empty())
      m_ShaderDesc.Name = std::filesystem::path(m_ShaderDesc.VertexPath).filename().string();

    // Reflection and layouts
    std::vector<ReflectionData> reflectionData = {};

    if (!m_VulkanSPIRV[vSS::eVertex].empty()) {
      const auto vertexReflectionData = GetReflectionData(m_VulkanSPIRV[vSS::eVertex]);
      reflectionData.emplace_back(vertexReflectionData);
    }
    if (!m_VulkanSPIRV[vSS::eFragment].empty()) {
      const auto fragReflectionData = GetReflectionData(m_VulkanSPIRV[vSS::eFragment]);
      reflectionData.emplace_back(fragReflectionData);
    }
    if (!m_VulkanSPIRV[vSS::eCompute].empty()) {
      const auto compReflectionData = GetReflectionData(m_VulkanSPIRV[vSS::eCompute]);
      reflectionData.emplace_back(compReflectionData);
    }

    m_DescriptorSetLayouts = CreateLayout(reflectionData);

    m_PipelineLayout = CreatePipelineLayout(m_DescriptorSetLayouts, reflectionData);

    // Free modules
    for (auto& module : m_ReflectModules)
      spvReflectDestroyShaderModule(&module);
    m_ReflectModules.clear();

    // Clear caches
    m_VulkanSourceCode.clear();
    m_VulkanFilePath.clear();
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
    else {
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
    SpvReflectResult result = spvReflectCreateShaderModule(spirvBytes.size(), spirvBytes.data(), &module);
    OX_CORE_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);

    // Descriptor sets
    uint32_t count = 0;
    result = spvReflectEnumerateDescriptorSets(&module, &count, nullptr);
    OX_CORE_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
    std::vector<SpvReflectDescriptorSet*> descriptorSets(count);
    result = spvReflectEnumerateDescriptorSets(&module, &count, descriptorSets.data());
    OX_CORE_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);

    // Block variables
    result = spvReflectEnumeratePushConstantBlocks(&module, &count, nullptr);
    OX_CORE_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
    std::vector<SpvReflectBlockVariable*> blockVariables(count);
    result = spvReflectEnumeratePushConstantBlocks(&module, &count, blockVariables.data());
    OX_CORE_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);

    m_ReflectModules.emplace_back(module);

    return {
      descriptorSets,
      blockVariables,
      (vk::ShaderStageFlagBits)module.shader_stage
    };
  }

  std::vector<vk::DescriptorSetLayout> VulkanShader::CreateLayout(const std::vector<ReflectionData>& reflectionData) const {
    std::vector<std::vector<vk::DescriptorSetLayoutBinding>> layoutBindings = {};
    for (const auto& [data, _, stage] : reflectionData) {
      for (const auto& descriptor : data) {
        std::vector<vk::DescriptorSetLayoutBinding> bindings = {};
        for (uint32_t i = 0; i < descriptor->binding_count; i++) {
          vk::DescriptorSetLayoutBinding binding;
          binding.binding = descriptor->bindings[i]->binding;
          binding.descriptorType = (vk::DescriptorType)descriptor->bindings[i]->descriptor_type;
          binding.stageFlags = stage;
          binding.descriptorCount = 1;
          bindings.emplace_back(binding);
        }
        layoutBindings.emplace_back(bindings);
      }
    }

    std::vector<vk::DescriptorSetLayout> layouts;

    for (auto& bindings : layoutBindings) {
      vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;
      descriptorSetLayoutCreateInfo.bindingCount = (uint32_t)bindings.size();
      descriptorSetLayoutCreateInfo.pBindings = bindings.data();
      vk::DescriptorSetLayout setLayout;
      const auto result = VulkanContext::GetDevice().createDescriptorSetLayout(&descriptorSetLayoutCreateInfo, nullptr, &setLayout);
      VulkanUtils::CheckResult(result);
      layouts.emplace_back(setLayout);
    }

    return layouts;
  }

  vk::PipelineLayout VulkanShader::CreatePipelineLayout(const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts,
                                                        const std::vector<ReflectionData>& reflectionData) const {
    vk::PipelineLayoutCreateInfo createInfo;
    createInfo.setLayoutCount = (uint32_t)descriptorSetLayouts.size();
    createInfo.pSetLayouts = descriptorSetLayouts.data();
    std::vector<vk::PushConstantRange> pushConstantRanges = {};
    for (auto& data : reflectionData) {
      for (auto& block : data.BlockVariables) {
        pushConstantRanges.emplace_back(vk::PushConstantRange{data.StageFlag, block->offset, block->size});
      }
    }
    createInfo.pPushConstantRanges = pushConstantRanges.data();
    createInfo.pushConstantRangeCount = pushConstantRanges.size();
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
      std::remove(cachedPath.string().c_str());
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

  void VulkanShader::UpdateDescriptorSets() const {
    const auto& LogicalDevice = VulkanContext::Context.Device;

    LogicalDevice.updateDescriptorSets(*DescriptorSets.data(), nullptr);
  }

  std::vector<vk::PipelineShaderStageCreateInfo>& VulkanShader::GetShaderStages() {
    if (m_ShaderStages.empty()) {
      m_ShaderStages = {GetVertexShaderStage(), GetFragmentShaderStage()};
    }
    return m_ShaderStages;
  }
}
