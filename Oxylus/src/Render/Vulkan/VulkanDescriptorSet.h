#pragma once

#include <vulkan/vulkan.hpp>

namespace Oxylus {
  class VulkanDescriptorSet {
  public:
    std::vector<vk::WriteDescriptorSet> WriteDescriptorSets;

    VulkanDescriptorSet() = default;

    void Allocate(const std::vector<vk::DescriptorSetLayout>& layouts, uint32_t layoutIndex = 0);

    void Update(bool waitForQueueIdle = false) const;

    void Destroy();

    const vk::DescriptorSet& Get() const {
      return m_DescriptorSet;
    }

  private:
    vk::DescriptorSet m_DescriptorSet;
  };
}
