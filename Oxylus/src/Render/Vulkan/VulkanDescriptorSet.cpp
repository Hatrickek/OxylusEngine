#include "src/oxpch.h"
#include "VulkanDescriptorSet.h"

#include "VulkanContext.h"
#include "VulkanRenderer.h"
#include "Utils/VulkanUtils.h"

namespace Oxylus {
  void VulkanDescriptorSet::Allocate(const std::vector<vk::DescriptorSetLayout>& layouts, uint32_t layoutIndex) {
    const auto& LogicalDevice = VulkanContext::Context.Device;

    vk::DescriptorSetAllocateInfo allocateInfo;
    allocateInfo.descriptorPool = VulkanRenderer::s_RendererContext.DescriptorPool;
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts = &layouts[layoutIndex];
    VulkanUtils::CheckResult(LogicalDevice.allocateDescriptorSets(&allocateInfo, &m_DescriptorSet));
  }

  void VulkanDescriptorSet::Update(bool waitForQueueIdle) const {
    const auto& LogicalDevice = VulkanContext::Context.Device;

    if (waitForQueueIdle)
      VulkanRenderer::WaitGraphicsQueueIdle();

    LogicalDevice.updateDescriptorSets(WriteDescriptorSets, nullptr);
  }

  void VulkanDescriptorSet::Destroy() { }
}
