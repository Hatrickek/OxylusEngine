#pragma once

#include <filesystem>
#include <functional>
#include <unordered_map>
#include <shaderc/shaderc.hpp>

#include <vulkan/vulkan.hpp>

#include "spirv_reflect.h"

namespace Oxylus {
  enum class DescriptorPropertyType {
    None = 0,
    Sampler2D,
    UniformBufer,
    StorageBuffer,
  };

  struct DesciptorProperty {
    DescriptorPropertyType Type;
    uint32_t BindingPoint;
    vk::DescriptorBufferInfo* DescriptorBufferInfo = nullptr;

    const vk::DescriptorImageInfo* DescriptorImageInfo = nullptr;
    uint32_t MaterialImageIndex;
  };

  enum class MaterialPropertyType {
    None = 0,
    Sampler2D,
    Bool,
    Int,
    Float,
    Float2,
    Float3,
    Float4,
  };

  struct MaterialProperty {
    MaterialPropertyType Type;

    std::string DisplayName;
    bool IsSlider = false;
    bool IsColor = false;
  };

  struct ShaderCI {
    std::string VertexPath;
    std::string FragmentPath;
    std::string EntryPoint = "main";
    std::string Name;
    std::string ComputePath;
    std::vector<DesciptorProperty> DesciptorProperties = {};
  };

  class VulkanShader {
  public:
    std::vector<vk::WriteDescriptorSet> DescriptorSets;

    VulkanShader() = default;
    VulkanShader(const ShaderCI& shaderCI);

    ~VulkanShader() = default;
    void Unload();
    void Destroy();

    void Reload();

    void OnReloadBegin(const std::function<void()>& event);
    void OnReloadEnd(const std::function<void()>& event);

    void UpdateDescriptorSets() const;

    unsigned GetStageCount() const { return static_cast<uint32_t>(m_ShaderStage.size()); }

    vk::PipelineShaderStageCreateInfo GetVertexShaderStage() { return m_ShaderStage[vk::ShaderStageFlagBits::eVertex]; }

    vk::PipelineShaderStageCreateInfo GetComputeShaderStage() { return m_ShaderStage[vk::ShaderStageFlagBits::eCompute]; }

    vk::PipelineShaderStageCreateInfo GetFragmentShaderStage() { return m_ShaderStage[vk::ShaderStageFlagBits::eFragment]; }

    const std::string& GetName() const { return m_ShaderDesc.Name; }

    std::vector<vk::PipelineShaderStageCreateInfo>& GetShaderStages();

    const ShaderCI& GetShaderDesc() const { return m_ShaderDesc; }

  private:
    std::unordered_map<vk::ShaderStageFlagBits, std::vector<uint32_t>> m_VulkanSPIRV;
    std::unordered_map<vk::ShaderStageFlagBits, std::string> m_VulkanSourceCode;
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

    std::filesystem::path GetCachedDirectory(vk::ShaderStageFlagBits stage, std::filesystem::path cacheDirectory);

    // Layouts
    std::vector<vk::DescriptorSetLayout> m_DescriptorSetLayouts = {};
    vk::PipelineLayout m_PipelineLayout;

    // Reflection
    struct ReflectionData {
      std::vector<SpvReflectDescriptorSet*> Bindings;
      std::vector<SpvReflectBlockVariable*> BlockVariables;
      vk::ShaderStageFlagBits StageFlag;
    };

    std::vector<SpvReflectShaderModule> m_ReflectModules = {};

    ReflectionData GetReflectionData(const std::vector<uint32_t>& spirvBytes);
    std::vector<vk::DescriptorSetLayout> CreateLayout(const std::vector<ReflectionData>& reflectionData) const;
    vk::PipelineLayout CreatePipelineLayout(const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts, const std::vector<ReflectionData>& reflectionData) const;
  };
}
