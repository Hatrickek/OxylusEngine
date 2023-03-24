#include "src/oxpch.h"
#include "VulkanFramebuffer.h"

#include "VulkanContext.h"
#include "VulkanRenderer.h"
#include "Utils/VulkanUtils.h"

namespace Oxylus {
  VulkanFramebuffer::VulkanFramebuffer(const FramebufferDescription& desc) {
    CreateFramebuffer(desc);
  }

  void VulkanFramebuffer::CreateFramebuffer(const FramebufferDescription& framebufferDescription) {
    const auto& LogicalDevice = VulkanContext::Context.Device;
    m_FramebufferDescription = framebufferDescription;

    m_DebugName = m_FramebufferDescription.DebugName;
    std::vector<vk::ImageView> fbAttachments;

    vk::FramebufferCreateInfo framebufferCreateInfo;
    if (m_FramebufferDescription.Extent) {
      framebufferCreateInfo.width = m_FramebufferDescription.Extent->width;
      framebufferCreateInfo.height = m_FramebufferDescription.Extent->height;
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

    framebufferCreateInfo.layers = 1;
    framebufferCreateInfo.renderPass = m_FramebufferDescription.RenderPass;
    framebufferCreateInfo.pAttachments = fbAttachments.data();
    framebufferCreateInfo.attachmentCount = (uint32_t)fbAttachments.size();

    VulkanUtils::CheckResult(LogicalDevice.createFramebuffer(&framebufferCreateInfo, nullptr, &m_Framebuffer));

    if (!m_Resizing && m_FramebufferDescription.Resizable)
      FrameBufferPool::AddToPool(this);
  }

  void VulkanFramebuffer::CreateFramebufferWithImageView(const FramebufferDescription& framebufferDescription,
                                                         vk::ImageView& view) {
    const auto& LogicalDevice = VulkanContext::Context.Device;
    m_FramebufferDescription = framebufferDescription;
    m_VkImageView = view;
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

    fbAttachments.emplace_back(view);

    framebufferCreateInfo.layers = 1;
    framebufferCreateInfo.renderPass = m_FramebufferDescription.RenderPass;
    framebufferCreateInfo.pAttachments = fbAttachments.data();
    framebufferCreateInfo.attachmentCount = (uint32_t)fbAttachments.size();

    VulkanUtils::CheckResult(LogicalDevice.createFramebuffer(&framebufferCreateInfo, nullptr, &m_Framebuffer));

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

    if (m_VkImageView)
      CreateFramebufferWithImageView(m_FramebufferDescription, m_VkImageView);
    else
      CreateFramebuffer(m_FramebufferDescription);
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
    LogicalDevice.destroyImageView(m_VkImageView);
  }

  std::vector<VulkanFramebuffer*> FrameBufferPool::m_Pool;

  void FrameBufferPool::ResizeBuffers() {
    for (const auto& framebuffer : m_Pool) {
      framebuffer->Resize();
    }
  }

  void FrameBufferPool::AddToPool(VulkanFramebuffer* framebuffer) {
    m_Pool.emplace_back(framebuffer);
  }

  void FrameBufferPool::RemoveFromPool(const std::string_view name) {
    const auto index = FindFramebuffer(name);
    m_Pool.erase(m_Pool.begin() + index);
  }

  uint32_t FrameBufferPool::FindFramebuffer(const std::string_view name) {
    for (size_t i = 0; i < m_Pool.size(); i++) {
      if (m_Pool[i]->GetDescription().DebugName == name) {
        return (uint32_t)i;
      }
    }
    return {};
  }
}
