#pragma once
#include <functional>
#include <vulkan/vulkan.hpp>

#include "VulkanImage.h"

namespace Oxylus {
  struct FramebufferDescription {
    std::string DebugName = "Framebuffer";
    std::vector<VulkanImageDescription> ImageDescription;
    vk::RenderPass RenderPass;
    uint32_t Width = 0;
    uint32_t Height = 0;
    vk::Extent2D* Extent; //If provieded OnResize is going to update the framebuffers width and height with this.
    std::function<void()> OnResize;
    bool Resizable = true;
  };

  class VulkanFramebuffer {
  public:
    VulkanFramebuffer() = default;
    VulkanFramebuffer(const FramebufferDescription& desc);

    ~VulkanFramebuffer() { }

    void CreateFramebuffer(const FramebufferDescription& framebufferDescription);
    void CreateFramebufferWithImageView(const FramebufferDescription& framebufferDescription, vk::ImageView& view);

    void Resize();
    void Destroy();

    const vk::Framebuffer& Get() const {
      return m_Framebuffer;
    }

    const std::vector<VulkanImage>& GetImage() const {
      return m_Images;
    }

    const FramebufferDescription& GetDescription() const {
      return m_FramebufferDescription;
    }

  private:
    FramebufferDescription m_FramebufferDescription;
    vk::Framebuffer m_Framebuffer;
    std::vector<VulkanImage> m_Images;
    std::string m_DebugName;
    vk::ImageView m_VkImageView;
    bool m_Resizing = false;
  };
}
