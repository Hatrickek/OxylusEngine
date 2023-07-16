#include "VulkanFramebuffer.h"
#include "Render/ResourcePool.h"

#include "VulkanContext.h"
#include "VulkanRenderer.h"
#include "Utils/VulkanUtils.h"

namespace Oxylus {
  VulkanFramebuffer::VulkanFramebuffer(const FramebufferDescription& desc) {
    CreateFramebuffer(desc);
  }

  void VulkanFramebuffer::CreateFramebuffer(const FramebufferDescription& framebufferDescription, vk::ImageView imageView) {
    const auto& LogicalDevice = VulkanContext::Context.Device;
    m_FramebufferDescription = framebufferDescription;
    m_ImageView = imageView;

    m_DebugName = m_FramebufferDescription.DebugName;
    std::vector<vk::ImageView> fbAttachments;

    vk::FramebufferCreateInfo framebufferCreateInfo;
    if (m_FramebufferDescription.Extent) {
      framebufferCreateInfo.width = m_FramebufferDescription.Extent->width;
      framebufferCreateInfo.height = m_FramebufferDescription.Extent->height;
    }
    else {
      framebufferCreateInfo.width = m_FramebufferDescription.Width;
      framebufferCreateInfo.height = m_FramebufferDescription.Height;
    }

    m_FramebufferDescription.Width = framebufferCreateInfo.width;
    m_FramebufferDescription.Height = framebufferCreateInfo.height;

    m_Images.clear();
    for (auto& id : m_FramebufferDescription.ImageDescription) {
      id.Width = framebufferCreateInfo.width;
      id.Height = framebufferCreateInfo.height;
      VulkanImage image(id);
      m_Images.emplace_back(image);
      fbAttachments.emplace_back(image.GetImageView());
    }

    if (imageView)
      fbAttachments.emplace_back(imageView);

    OX_CORE_ASSERT(m_FramebufferDescription.RenderPass)

    framebufferCreateInfo.layers = 1;
    framebufferCreateInfo.renderPass = m_FramebufferDescription.RenderPass;
    framebufferCreateInfo.pAttachments = fbAttachments.data();
    framebufferCreateInfo.attachmentCount = (uint32_t)fbAttachments.size();
    VulkanUtils::CheckResult(LogicalDevice.createFramebuffer(&framebufferCreateInfo, nullptr, &m_Framebuffer));
    VulkanUtils::SetObjectName((uint64_t)(VkFramebuffer)m_Framebuffer,
      vk::ObjectType::eFramebuffer,
      m_FramebufferDescription.DebugName.c_str());

    if (!m_Resizing && m_FramebufferDescription.Resizable)
      FrameBufferPool::AddToPool(this);
  }

  void VulkanFramebuffer::Resize() {
    const auto& LogicalDevice = VulkanContext::Context.Device;
    m_Resizing = true;

    VulkanRenderer::WaitDeviceIdle();
    for (auto& image : m_Images)
      image.Destroy();
    m_Images.clear();
    LogicalDevice.destroyFramebuffer(m_Framebuffer);

    CreateFramebuffer(m_FramebufferDescription, m_ImageView);

    if (m_FramebufferDescription.OnResize)
      m_FramebufferDescription.OnResize();
    m_Resizing = false;
  }

  void VulkanFramebuffer::Destroy() {
    const auto& LogicalDevice = VulkanContext::Context.Device;
    VulkanRenderer::WaitDeviceIdle();
    for (auto& image : m_Images)
      image.Destroy();
    m_Images.clear();
    LogicalDevice.destroyFramebuffer(m_Framebuffer);
    FrameBufferPool::RemoveFromPool(GetDescription().DebugName);
  }
}
