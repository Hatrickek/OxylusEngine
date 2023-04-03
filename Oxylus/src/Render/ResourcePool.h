#pragma once

#include "Render/Vulkan/VulkanFramebuffer.h"

namespace Oxylus {

  class FrameBufferPool {
  public:
    static void ResizeBuffers();
    static void AddToPool(VulkanFramebuffer* framebuffer);
    static void RemoveFromPool(std::string_view name);

    static std::vector<VulkanFramebuffer*>& GetPool() { return m_Pool; }

  private:
    static std::vector<VulkanFramebuffer*> m_Pool;
    static uint32_t FindFramebuffer(std::string_view name);
  };

  class ImagePool {
  public:
    static void ResizeImages();
    static void AddToPool(VulkanImage* image, vk::Extent2D* extent, const std::function<void()>& onresize);
    static void RemoveFromPool(std::string_view name);

  private:
    struct ImageResource {
      VulkanImage* Image;
      vk::Extent2D* Extent;
      const std::function<void()> OnResize;
    };
    static std::vector<ImageResource> m_Pool;
    static uint32_t FindImage(std::string_view name);
  };
  
} 
