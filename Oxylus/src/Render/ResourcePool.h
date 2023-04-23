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
    static void AddToPool(VulkanImage* image, vk::Extent2D* extent, const std::function<void()>& onresize, uint32_t extentMultiplier = 1);
    static void RemoveFromPool(std::string_view name);

  private:
    struct ImageResource {
      VulkanImage* Image;
      vk::Extent2D* Extent;
      const std::function<void()> OnResize;
      uint32_t ExtentMultiplier = 1;
    };
    static std::vector<ImageResource> m_Pool;
    static uint32_t FindImage(std::string_view name);
  };
  
} 
