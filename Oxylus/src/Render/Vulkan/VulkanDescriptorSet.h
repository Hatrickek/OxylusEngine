#pragma once

#include <vulkan/vulkan.hpp>

namespace Oxylus {
  class VulkanPipeline;

  struct SetDescription {
    uint32_t Binding = 0;
    uint32_t ArrayElemet = 0;
    uint32_t DescriptorCount = 1;
    vk::DescriptorType DescriptorType = {};
    vk::ShaderStageFlags ShaderStage;
    const vk::DescriptorImageInfo* pImageInfo = nullptr;
    const vk::DescriptorBufferInfo* pBufferInfo = nullptr;
    const vk::BufferView* pTexelBufferView = nullptr;
  };

  class VulkanDescriptorSet {
  public:
    std::vector<vk::WriteDescriptorSet> WriteDescriptorSets;

    VulkanDescriptorSet() = default;

    VulkanDescriptorSet& Allocate(const std::vector<vk::DescriptorSetLayout>& layouts, uint32_t layoutIndex = 0);
    VulkanDescriptorSet& CreateFromPipeline(const VulkanPipeline& pipeline, uint32_t layoutIndex = 0);
    void Update(bool waitForQueueIdle = false) const;
    void Destroy();

    const vk::DescriptorSet& Get() const { return m_DescriptorSet; }

  private:
    vk::DescriptorSet m_DescriptorSet;
  };
}
