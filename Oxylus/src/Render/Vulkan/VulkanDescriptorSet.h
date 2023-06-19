#pragma once

#include <vulkan/vulkan.hpp>

#include "VulkanShader.h"
#include "Core/Base.h"

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

    VulkanDescriptorSet& CreateFromShader(const Ref<VulkanShader>& shader, uint32_t set = 0);
    void Update(bool waitForQueueIdle = false) const;

    vk::DescriptorSet& Get() { return m_DescriptorSet; }
  private:
    vk::DescriptorSet m_DescriptorSet;
  };
}
