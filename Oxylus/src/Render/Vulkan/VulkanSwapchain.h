#pragma once

#include <functional>

#include <vulkan/vulkan.hpp>

#include "VulkanImage.h"
#include "VulkanRenderPass.h"

namespace Oxylus {
  class VulkanSwapchain {
  public:
    struct SwapChainSupportDetails {
      vk::SurfaceCapabilitiesKHR capabilities;
      std::vector<vk::SurfaceFormatKHR> formats;
      std::vector<vk::PresentModeKHR> present_modes;
    };

    uint32_t CurrentFrame = 0;
    uint32_t ImageIndex = 0;
    vk::SwapchainKHR m_SwapChain;
    vk::Extent2D m_Extent;
    vk::Format m_ImageFormat;
    vk::Format m_DepthFormat = vk::Format::eUndefined;
    uint8_t m_DepthChannelCount;
    std::vector<vk::Image> m_Images;
    uint32_t m_ImageCount;
    std::vector<vk::ImageView> m_ImageViews;
    uint32_t MaxFramesInFlight;
    VulkanRenderPass m_RenderPass;
    std::vector<vk::Framebuffer> m_FrameBuffers;
    vk::PresentModeKHR m_PresentMode = vk::PresentModeKHR::eImmediate;
    vk::SurfaceFormatKHR m_SurfaceFormat;
    SwapChainSupportDetails m_SupportDetails;
    std::vector<vk::Semaphore> ImageAcquiredSemaphores;
    std::vector<vk::Semaphore> RenderCompleteSemaphores;
    std::vector<vk::Fence> InFlightFences;
    vk::Fence* ImagesInFlight[3];
    std::vector<VulkanCommandBuffer> CommandBuffers;
    bool Resizing;
    bool IsOutOfDate;

    VulkanSwapchain() = default;
    ~VulkanSwapchain();

    void CreateSwapChain();
    void RecreateSwapChain();
    VulkanSwapchain* Submit();
    VulkanSwapchain* SubmitPass(const std::function<void(VulkanCommandBuffer& commandBuffer)>& func);
    void Present();
    void ClearSwapChain();
    bool AcquireNextImage();
    void SetVsync(bool isVsync = true, bool recreate = true);
    bool ToggleVsync(); //Returns true on Fifo mode. Otherwise false.
    VulkanCommandBuffer& GetCommandBuffer();
  };
}
