#include "src/oxpch.h"
#include "VulkanRenderPass.h"

#include "VulkanContext.h"
#include "Utils/VulkanUtils.h"

namespace Oxylus {
  VulkanRenderPass::~VulkanRenderPass() { }

  void VulkanRenderPass::CreateRenderPass(const vk::RenderPassCreateInfo& renderPassCI) {
    const auto& LogicalDevice = VulkanContext::GetDevice();
    m_RenderPass = LogicalDevice.createRenderPass(renderPassCI).value;
  }

  void VulkanRenderPass::CreateRenderPass(const RenderPassDescription& description) {
    OX_CORE_ASSERT(description.Name.empty());
    m_Description = description;
    const auto& LogicalDevice = VulkanContext::GetDevice();
    m_RenderPass = LogicalDevice.createRenderPass(description.CreateInfo).value;
    VulkanUtils::SetObjectName((uint64_t)(VkRenderPass)m_RenderPass, vk::ObjectType::eRenderPass, m_Description.Name.c_str());
  }

  void VulkanRenderPass::Destroy() const {
    const auto& LogicalDevice = VulkanContext::Context.Device;
    LogicalDevice.destroyRenderPass(Get());
  }
}
