#pragma once

#include <filesystem>
#include <functional>
#include <unordered_map>
#include <shaderc/shaderc.hpp>

#include <vulkan/vulkan.hpp>

#include "spirv_reflect.h"

namespace Oxylus {
  struct ShaderCI {
    std::string VertexPath;
    std::string FragmentPath;
    std::string EntryPoint = "main";
    std::string Name;
    std::string ComputePath;
  };

  class VulkanShader {
  public:
    VulkanShader() = default;
    VulkanShader(const ShaderCI& shaderCI);

    ~VulkanShader() = default;
    void Unload();
    void Destroy();

    void Reload();

    void OnReloadBegin(const std::function<void()>& event);
    void OnReloadEnd(const std::function<void()>& event);

    unsigned GetStageCount() const { return static_cast<uint32_t>(m_ShaderStage.size()); }

    vk::PipelineShaderStageCreateInfo GetVertexShaderStage() { return m_ShaderStage[vk::ShaderStageFlagBits::eVertex]; }
    vk::PipelineShaderStageCreateInfo GetComputeShaderStage() { return m_ShaderStage[vk::ShaderStageFlagBits::eCompute]; }
    vk::PipelineShaderStageCreateInfo GetFragmentShaderStage() { return m_ShaderStage[vk::ShaderStageFlagBits::eFragment]; }
    std::vector<vk::PipelineShaderStageCreateInfo>& GetShaderStages();

    const std::string& GetName() const { return m_ShaderDesc.Name; }
    const ShaderCI& GetShaderDesc() const { return m_ShaderDesc; }

    vk::PipelineLayout& GetPipelineLayout() { return m_PipelineLayout; }
    std::vector<vk::DescriptorSetLayout>& GetDescriptorSetLayouts() { return m_DescriptorSetLayouts; }
    std::vector<vk::WriteDescriptorSet> GetWriteDescriptorSets(const vk::DescriptorSet& descriptorSet, uint32_t set = 0);

  private:
    std::unordered_map<vk::ShaderStageFlagBits, std::vector<uint32_t>> m_VulkanSPIRV;
    std::unordered_map<vk::ShaderStageFlagBits, std::filesystem::path> m_VulkanFilePath;
    std::unordered_map<vk::ShaderStageFlagBits, vk::ShaderModule> m_ShaderModule;
    std::unordered_map<vk::ShaderStageFlagBits, vk::PipelineShaderStageCreateInfo> m_ShaderStage;
    std::vector<vk::PipelineShaderStageCreateInfo> m_ShaderStages;
    ShaderCI m_ShaderDesc;
    std::function<void()> m_OnReloadBeginEvent;
    std::function<void()> m_OnReloadEndEvent;
    bool m_Loaded = false;

    void CreateShader();
    void ReadOrCompile(vk::ShaderStageFlagBits stage, const std::string& source, const shaderc::CompileOptions& options);
    void CreateShaderModule(vk::ShaderStageFlagBits stage, const std::vector<unsigned>& source);

    std::filesystem::path GetCachedDirectory(vk::ShaderStageFlagBits stage, const std::filesystem::path& cacheDirectory);

    // Layouts
    std::vector<vk::DescriptorSetLayout> m_DescriptorSetLayouts = {};
    vk::PipelineLayout m_PipelineLayout;

    std::unordered_map<uint32_t, std::vector<vk::WriteDescriptorSet>> m_WriteDescriptorSets = {};

    // Reflection
    struct PushConstantData {
      uint32_t Size = 0;
      uint32_t Offset = 0;
      vk::ShaderStageFlagBits Stage = {};
    };

    struct DescriptorBindingData {
      std::string name;
      uint32_t binding;
      uint32_t set;
      vk::DescriptorType descriptor_type;
      uint32_t count;
    };

    struct DescriptorSetData {
      uint32_t set;
      uint32_t binding_count;
      std::vector<DescriptorBindingData> bindings;
    };

    struct ReflectionData {
      std::vector<DescriptorSetData> Bindings = {};
      std::vector<PushConstantData> PushConstants = {};
      vk::ShaderStageFlags Stage = {};
    };

    std::vector<ReflectionData> m_ReflectionData = {};
    std::vector<SpvReflectShaderModule> m_ReflectModules = {};

    ReflectionData GetReflectionData(const std::vector<uint32_t>& spirvBytes);
    void CreateLayoutBindings(const ReflectionData& reflectionData, std::vector<std::vector<vk::DescriptorSetLayoutBinding>>& layoutBindings);
    std::vector<vk::DescriptorSetLayout> CreateLayout(const std::vector<std::vector<vk::DescriptorSetLayoutBinding>>& layoutBindings);
    vk::PipelineLayout CreatePipelineLayout(const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts) const;
  };
}
