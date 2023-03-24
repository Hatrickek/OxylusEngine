#include "oxpch.h"
#include "VulkanShader.h"
#include "VulkanContext.h"

#include <filesystem>
#include <fstream>
#include <ranges>

#include "VulkanRenderer.h"
#include "utils/Log.h"
#include "Utils/Profiler.h"
#include "Utils/FileUtils.h"

namespace Oxylus {
  struct Includer : shaderc::CompileOptions::IncluderInterface {
    shaderc_include_result* GetInclude(const char* requested_source,
                                       shaderc_include_type type,
                                       const char* requesting_source,
                                       size_t include_depth) override {
      const std::string name = std::string(requested_source);
      const std::string filepath = std::string(std::format("Resources/Shaders/{0}", name));
      const std::string content = FileUtils::ReadFile(filepath);

      const auto container = new std::array<std::string, 2>;
      (*container)[0] = name;
      (*container)[1] = content;

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
    return "resources/shaders/compiled/";
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
      OX_CORE_ASSERT(!content.empty())
      m_VulkanSourceCode[vk::ShaderStageFlagBits::eCompute] = content;
    }
    if (!m_ShaderDesc.VertexPath.empty()) {
      m_VulkanFilePath[vk::ShaderStageFlagBits::eVertex] = m_ShaderDesc.VertexPath;
      const auto& content = FileUtils::ReadFile(m_ShaderDesc.VertexPath);
      OX_CORE_ASSERT(!content.empty())
      m_VulkanSourceCode[vk::ShaderStageFlagBits::eVertex] = content;
    }
    if (!m_ShaderDesc.FragmentPath.empty()) {
      m_VulkanFilePath[vk::ShaderStageFlagBits::eFragment] = m_ShaderDesc.FragmentPath;
      const auto& content = FileUtils::ReadFile(m_ShaderDesc.FragmentPath);
      OX_CORE_ASSERT(!content.empty())
      m_VulkanSourceCode[vk::ShaderStageFlagBits::eFragment] = content;
    }

    shaderc::CompileOptions options;
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
    options.SetTargetSpirv(shaderc_spirv_version_1_6);
#ifdef OX_RELEASE
    options.SetOptimizationLevel(shaderc_optimization_level_performance);
#else
    options.SetOptimizationLevel(shaderc_optimization_level_zero);
#endif
    options.SetIncluder(std::make_unique<Includer>());
    m_VulkanSPIRV.clear();

    std::filesystem::path cacheDirectory = GetCacheDirectory();
    for (auto&& [stage, source] : m_VulkanSourceCode) {
      ReadOrCompile(stage, source, options);
    }
    for (auto&& [stage, source] : m_VulkanSPIRV) {
      CreateShaderModule(stage, source);
    }

    if (m_ShaderDesc.Name.empty())
      m_ShaderDesc.Name = std::filesystem::path(m_ShaderDesc.VertexPath).filename().string();

    timer.Print(std::format("Shader {0} loaded", m_ShaderDesc.Name));

    m_Loaded = true;
  }

  std::filesystem::path VulkanShader::GetCachedDirectory(vk::ShaderStageFlagBits stage,
                                                         std::filesystem::path cacheDirectory) {
    return cacheDirectory / (m_VulkanFilePath[stage].filename().string() + GetCompiledFileExtension(stage));
  }

  void VulkanShader::ReadOrCompile(vk::ShaderStageFlagBits stage,
                                   const std::string& source,
                                   shaderc::CompileOptions options) {
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

  void VulkanShader::CreateDescriptorSet() {
    for (auto& prop : GetMaterialProperties()) {
      DescriptorSets.emplace_back(vk::WriteDescriptorSet{});
    }
  }

  void VulkanShader::Unload() {
    if (!m_Loaded)
      return;
    for (const auto& stage : m_VulkanSPIRV | std::views::keys) {
      VulkanContext::Context.Device.destroyShaderModule(m_ShaderModule[stage], nullptr);
    }
    m_ShaderStages.clear();
    m_Loaded = false;
  }

  void VulkanShader::Destroy() {
    Unload();
    VulkanRenderer::RemoveShader(GetName());
  }

  void VulkanShader::Reload() {
    VulkanRenderer::WaitDeviceIdle();
    //Delete compiled files
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
