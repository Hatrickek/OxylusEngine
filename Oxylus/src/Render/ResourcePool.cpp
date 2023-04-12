#include "ResourcePool.h"

namespace Oxylus {

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

  std::vector<ImagePool::ImageResource> ImagePool::m_Pool;

  void ImagePool::ResizeImages() {
    for (const auto& [image, extent, onresize] : m_Pool) {
      image->Destroy();
      VulkanImageDescription desc = image->GetDesc();
      if (extent) {
        desc.Width = extent->width;
        desc.Height = extent->height;
      }
      image->Create(desc);
      if (onresize)
        onresize();
    }
  }

  void ImagePool::AddToPool(VulkanImage* image, vk::Extent2D* extent, const std::function<void()>& onresize) {
    m_Pool.emplace_back(ImageResource{image, extent, onresize});
  }

  void ImagePool::RemoveFromPool(const std::string_view name) {
    OX_CORE_ERROR("Not implemented");
  }

  uint32_t ImagePool::FindImage(const std::string_view name) {
    OX_CORE_ERROR("Not implemented");
    return {};
  }
} 