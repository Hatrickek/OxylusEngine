#include "VulkanDescriptorSet.h"

#include "VulkanContext.h"
#include "VulkanRenderer.h"
#include "Utils/VulkanUtils.h"
#include "VulkanPipeline.h"

namespace Oxylus {
  VulkanDescriptorSet& VulkanDescriptorSet::CreateFromShader(const Ref<VulkanShader>& shader, const uint32_t set) {
    vk::DescriptorSetAllocateInfo allocateInfo;
    allocateInfo.descriptorPool = DescriptorPoolManager::Get()->GetFreePool();
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts = &shader->GetDescriptorSetLayouts()[set];
    const auto result1 = VulkanContext::GetDevice().allocateDescriptorSets(allocateInfo);
    VulkanUtils::CheckResult(result1.result);
    m_DescriptorSet = result1.value[0];

    WriteDescriptorSets = shader->GetWriteDescriptorSets(m_DescriptorSet, set);

    return *this;
  }

  void VulkanDescriptorSet::Update(bool waitForQueueIdle) const {
    const auto& LogicalDevice = VulkanContext::Context.Device;

    if (waitForQueueIdle)
      VulkanRenderer::WaitGraphicsQueueIdle();

    LogicalDevice.updateDescriptorSets(WriteDescriptorSets, nullptr);
  }
}
