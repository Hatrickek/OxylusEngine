#include "oxpch.h"
#include "VulkanRenderPass.h"

#include "VulkanContext.h"

namespace Oxylus {
  VulkanRenderPass::~VulkanRenderPass() { }

  void VulkanRenderPass::CreateRenderPass(const vk::RenderPassCreateInfo& renderPassCI) {
    m_Description = renderPassCI;
    const auto& LogicalDevice = VulkanContext::Context.Device;

    m_RenderPass = LogicalDevice.createRenderPass(renderPassCI);
  }

  void VulkanRenderPass::Destroy() const {
    const auto& LogicalDevice = VulkanContext::Context.Device;
    LogicalDevice.destroyRenderPass(Get());
  }
}
