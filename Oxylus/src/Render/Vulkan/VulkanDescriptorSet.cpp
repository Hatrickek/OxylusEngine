#include "src/oxpch.h"
#include "VulkanDescriptorSet.h"

#include "VulkanContext.h"
#include "VulkanRenderer.h"
#include "Utils/VulkanUtils.h"
#include "VulkanPipeline.h"

namespace Oxylus {
  VulkanDescriptorSet& VulkanDescriptorSet::Allocate(const std::vector<vk::DescriptorSetLayout>& layouts,
                                                     uint32_t layoutIndex) {
    const auto& LogicalDevice = VulkanContext::Context.Device;

    vk::DescriptorSetAllocateInfo allocateInfo;
    allocateInfo.descriptorPool = VulkanRenderer::s_RendererContext.DescriptorPool;
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts = &layouts[layoutIndex];
    VulkanUtils::CheckResult(LogicalDevice.allocateDescriptorSets(&allocateInfo, &m_DescriptorSet));

    return *this;
  }

  VulkanDescriptorSet& VulkanDescriptorSet::CreateFromPipeline(const VulkanPipeline& pipeline, uint32_t layoutIndex) {
    Allocate(pipeline.GetDescriptorSetLayout(), layoutIndex);
    auto& layout = pipeline.GetDesc().SetDescriptions[layoutIndex];
    for (auto& binding : layout) {
      WriteDescriptorSets.emplace_back(
        m_DescriptorSet,
        binding.Binding,
        binding.ArrayElemet,
        binding.DescriptorCount,
        binding.DescriptorType,
        binding.pImageInfo,
        binding.pBufferInfo
      );
    }
    return *this;
  }

  void VulkanDescriptorSet::Update(bool waitForQueueIdle) const {
    const auto& LogicalDevice = VulkanContext::Context.Device;

    if (waitForQueueIdle)
      VulkanRenderer::WaitGraphicsQueueIdle();

    LogicalDevice.updateDescriptorSets(WriteDescriptorSets, nullptr);
  }

  void VulkanDescriptorSet::Destroy() { }
}
