#include "src/oxpch.h"
#include "VulkanSwapchain.h"
#include "VulkanContext.h"
#include "VulkanRenderer.h"
#include "Utils/VulkanUtils.h"
#include "Render/Window.h"

namespace Oxylus {
  VulkanSwapchain::~VulkanSwapchain() {}

  static vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& avaliableFormats) {
    if (avaliableFormats.size() == 1 && avaliableFormats[0].format == vk::Format::eUndefined) {
      return {vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear};
    }

    for (const auto& av_format : avaliableFormats) {
      if (av_format.format == vk::Format::eB8G8R8A8Unorm && av_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
        return av_format;
      }
    }

    return avaliableFormats[0];
  }

  void VulkanSwapchain::CreateSwapChain() {
    auto& PhysicalDevice = VulkanContext::Context.PhysicalDevice;
    auto& Surface = VulkanContext::Context.Surface;
    auto& LogicalDevice = VulkanContext::Context.Device;

    VulkanUtils::CheckResult(PhysicalDevice.getSurfaceCapabilitiesKHR(Surface, &m_SupportDetails.capabilities));
    uint32_t format_count;
    VulkanUtils::CheckResult(PhysicalDevice.getSurfaceFormatsKHR(Surface, &format_count, nullptr));
    if (format_count != 0) {
      m_SupportDetails.formats.resize(format_count);
      VulkanUtils::CheckResult(
        PhysicalDevice.getSurfaceFormatsKHR(Surface, &format_count, m_SupportDetails.formats.data()));
    }
    uint32_t present_mode_count;
    VulkanUtils::CheckResult(PhysicalDevice.getSurfacePresentModesKHR(Surface, &present_mode_count, nullptr));
    if (present_mode_count != 0) {
      m_SupportDetails.present_modes.resize(present_mode_count);
      VulkanUtils::CheckResult(PhysicalDevice.getSurfacePresentModesKHR(Surface,
        &present_mode_count,
        m_SupportDetails.present_modes.data()));
    }

    m_SurfaceFormat = ChooseSwapSurfaceFormat(m_SupportDetails.formats);
    vk::Extent2D actualExtent = Window::GetWindowExtent();

    actualExtent.width = std::max(m_SupportDetails.capabilities.minImageExtent.width,
      std::min(m_SupportDetails.capabilities.maxImageExtent.width, actualExtent.width));
    actualExtent.height = std::max(m_SupportDetails.capabilities.minImageExtent.height,
      std::min(m_SupportDetails.capabilities.maxImageExtent.height, actualExtent.height));

    uint32_t desiredNumberOfSwapchainImages = m_SupportDetails.capabilities.minImageCount + 1;
    if ((m_SupportDetails.capabilities.maxImageCount > 0) && (desiredNumberOfSwapchainImages > m_SupportDetails.
                                                              capabilities.maxImageCount)) {
      desiredNumberOfSwapchainImages = m_SupportDetails.capabilities.maxImageCount;
    }

    vk::SurfaceTransformFlagBitsKHR preTransform;
    if (m_SupportDetails.capabilities.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity) {
      preTransform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
    }
    else {
      preTransform = m_SupportDetails.capabilities.currentTransform;
    }
    const vk::CompositeAlphaFlagBitsKHR compositeAlpha = (m_SupportDetails.capabilities.supportedCompositeAlpha &
                                                          vk::CompositeAlphaFlagBitsKHR::ePreMultiplied)
                                                           ? vk::CompositeAlphaFlagBitsKHR::ePreMultiplied
                                                           : (m_SupportDetails.capabilities.supportedCompositeAlpha &
                                                              vk::CompositeAlphaFlagBitsKHR::ePostMultiplied)
                                                           ? vk::CompositeAlphaFlagBitsKHR::ePostMultiplied
                                                           : (m_SupportDetails.capabilities.supportedCompositeAlpha &
                                                              vk::CompositeAlphaFlagBitsKHR::eInherit)
                                                           ? vk::CompositeAlphaFlagBitsKHR::eInherit
                                                           : vk::CompositeAlphaFlagBitsKHR::eOpaque;

    const vk::SwapchainKHR oldSwapchain = m_SwapChain;

    vk::SwapchainCreateInfoKHR swapchainCI;
    swapchainCI.surface = Surface;
    swapchainCI.minImageCount = desiredNumberOfSwapchainImages;
    swapchainCI.imageFormat = m_SurfaceFormat.format;
    swapchainCI.imageColorSpace = m_SurfaceFormat.colorSpace;
    swapchainCI.imageExtent = actualExtent;
    swapchainCI.imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst;
    swapchainCI.preTransform = preTransform;
    swapchainCI.imageArrayLayers = 1;
    swapchainCI.imageSharingMode = vk::SharingMode::eExclusive;
    swapchainCI.pQueueFamilyIndices = nullptr;
    const uint32_t queueFamilyIndices[2] = {
      (VulkanContext::VulkanQueue.graphicsQueueFamilyIndex), (VulkanContext::VulkanQueue.presentQueueFamilyIndex)
    };
    if (VulkanContext::VulkanQueue.graphicsQueueFamilyIndex != VulkanContext::VulkanQueue.presentQueueFamilyIndex) {
      swapchainCI.imageSharingMode = vk::SharingMode::eConcurrent;
      swapchainCI.queueFamilyIndexCount = 2;
      swapchainCI.pQueueFamilyIndices = queueFamilyIndices;
    }
    swapchainCI.presentMode = m_PresentMode;
    swapchainCI.oldSwapchain = oldSwapchain;
    swapchainCI.clipped = VK_TRUE;
    swapchainCI.compositeAlpha = compositeAlpha;

    m_SwapChain = LogicalDevice.createSwapchainKHR(swapchainCI).value;

    m_Extent = actualExtent;
    m_ImageFormat = m_SurfaceFormat.format;

    VulkanUtils::CheckResult(LogicalDevice.getSwapchainImagesKHR(m_SwapChain, &m_ImageCount, nullptr));
    m_Images.resize(m_ImageCount);

    VulkanUtils::CheckResult(LogicalDevice.getSwapchainImagesKHR(m_SwapChain, &m_ImageCount, m_Images.data()));

    m_ImageViews.reserve(m_Images.size());

    MaxFramesInFlight = m_ImageCount - 1;

    for (const auto& view : m_ImageViews) {
      LogicalDevice.destroyImageView(view);
    }
    m_ImageViews.clear();

    for (auto& m_Image : m_Images) {
      vk::ImageView tmpImageview;
      vk::ImageViewCreateInfo viewInfo = {};
      viewInfo.image = m_Image;
      viewInfo.format = m_ImageFormat;
      viewInfo.viewType = vk::ImageViewType::e2D;
      viewInfo.components = {
        VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A
      };
      viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
      viewInfo.subresourceRange.baseMipLevel = 0;
      viewInfo.subresourceRange.levelCount = 1;
      viewInfo.subresourceRange.baseArrayLayer = 0;
      viewInfo.subresourceRange.layerCount = 1;

      VulkanUtils::CheckResult(LogicalDevice.createImageView(&viewInfo, nullptr, &tmpImageview));

      m_ImageViews.emplace_back(tmpImageview);

      CommandBuffers.resize(m_ImageCount);
      for (uint32_t i = 0; i < m_ImageCount; i++) {
        CommandBuffers[i].CreateBuffer();
      }
    }

    constexpr uint64_t candidate_count = 3;
    vk::Format candidates[3] = {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint};

    uint8_t sizes[3] = {4, 4, 3};

    vk::FormatFeatureFlagBits flags = vk::FormatFeatureFlagBits::eDepthStencilAttachment;
    for (uint64_t i = 0; i < candidate_count; ++i) {
      vk::FormatProperties properties;
      PhysicalDevice.getFormatProperties(candidates[i], &properties);

      if ((properties.linearTilingFeatures & flags) == flags) {
        m_DepthFormat = candidates[i];
        m_DepthChannelCount = sizes[i];
      }
      else if ((properties.optimalTilingFeatures & flags) == flags) {
        m_DepthFormat = candidates[i];
        m_DepthChannelCount = sizes[i];
      }
    }

    std::array<vk::AttachmentDescription, 1> attachments;
    attachments[0].format = m_SurfaceFormat.format;
    attachments[0].samples = vk::SampleCountFlagBits::e1;
    attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
    attachments[0].storeOp = vk::AttachmentStoreOp::eStore;
    attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    attachments[0].initialLayout = vk::ImageLayout::eUndefined;
    attachments[0].finalLayout = vk::ImageLayout::ePresentSrcKHR;

    std::vector<vk::AttachmentReference> colorAttachmentReferences;
    {
      vk::AttachmentReference colorReference;
      colorReference.attachment = 0;
      colorReference.layout = vk::ImageLayout::eColorAttachmentOptimal;
      colorAttachmentReferences.push_back(colorReference);
    }

    using vPSFB = vk::PipelineStageFlagBits;
    using vAFB = vk::AccessFlagBits;
    std::vector<vk::SubpassDependency> subpassDependencies{
      {
        0, VK_SUBPASS_EXTERNAL, vPSFB::eColorAttachmentOutput, vPSFB::eBottomOfPipe,
        vAFB::eColorAttachmentRead | vAFB::eColorAttachmentWrite, vAFB::eMemoryRead, vk::DependencyFlagBits::eByRegion
      },
      {
        VK_SUBPASS_EXTERNAL, 0, vPSFB::eBottomOfPipe, vPSFB::eColorAttachmentOutput, vAFB::eMemoryRead,
        vAFB::eColorAttachmentRead | vAFB::eColorAttachmentWrite, vk::DependencyFlagBits::eByRegion
      },
    };
    std::vector<vk::SubpassDescription> subpasses{
      {
        {}, vk::PipelineBindPoint::eGraphics,
        // Input attachment references
        0, nullptr,
        // Color / resolve attachment references
        static_cast<uint32_t>(colorAttachmentReferences.size()), colorAttachmentReferences.data(), nullptr, nullptr,
        // Preserve attachments
        0, nullptr
      },
    };

    subpassDependencies.clear();

    vk::SubpassDependency subpassDependency;
    subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    subpassDependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    subpassDependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    subpassDependencies.emplace_back(subpassDependency);

    vk::RenderPassCreateInfo renderPassInfo;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = static_cast<uint32_t>(subpasses.size());
    renderPassInfo.pSubpasses = subpasses.data();
    renderPassInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
    renderPassInfo.pDependencies = subpassDependencies.data();
    m_RenderPass.CreateRenderPass(renderPassInfo);

    vk::ImageView fbAttachments[1];

    vk::FramebufferCreateInfo FramebufferCI;
    FramebufferCI.pAttachments = fbAttachments;
    FramebufferCI.attachmentCount = 1;
    FramebufferCI.height = m_Extent.height;
    FramebufferCI.width = m_Extent.width;
    FramebufferCI.renderPass = m_RenderPass.Get();
    FramebufferCI.layers = 1;

    m_FrameBuffers.clear();
    m_FrameBuffers.resize(m_ImageCount);
    for (size_t i = 0; i < m_FrameBuffers.size(); i++) {
      fbAttachments[0] = m_ImageViews[i];
      VulkanUtils::CheckResult(LogicalDevice.createFramebuffer(&FramebufferCI, nullptr, &m_FrameBuffers[i]));
    }

    ImageAcquiredSemaphores.clear();
    RenderCompleteSemaphores.clear();
    ImageAcquiredSemaphores.resize(MaxFramesInFlight);
    RenderCompleteSemaphores.resize(MaxFramesInFlight);
    InFlightFences.resize(MaxFramesInFlight);
    for (uint32_t i = 0; i < MaxFramesInFlight; i++) {
      vk::SemaphoreCreateInfo semaphoreInfo;
      VulkanUtils::CheckResult(LogicalDevice.createSemaphore(&semaphoreInfo, nullptr, &ImageAcquiredSemaphores[i]));
      VulkanUtils::CheckResult(LogicalDevice.createSemaphore(&semaphoreInfo, nullptr, &RenderCompleteSemaphores[i]));

      const vk::FenceCreateInfo fenceCreateInfo = {vk::FenceCreateFlagBits::eSignaled};
      VulkanUtils::CheckResult(LogicalDevice.createFence(&fenceCreateInfo, nullptr, &InFlightFences[i]));
      for (auto& fence : ImagesInFlight) {
        fence = nullptr;
      }
    }

    if (oldSwapchain) {
      LogicalDevice.destroySwapchainKHR(oldSwapchain);
    }
  }

  void VulkanSwapchain::RecreateSwapChain() {
    if (Window::IsMinimized())
      return;
    VulkanRenderer::WaitDeviceIdle();
    ClearSwapChain();
    CreateSwapChain();
    Resizing = false;
  }

  void VulkanSwapchain::Submit() {
    ZoneScoped;
    //Swapchain pass submit
    vk::SubmitInfo submitInfo;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &GetCommandBuffer().Get();
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &RenderCompleteSemaphores[CurrentFrame];
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &ImageAcquiredSemaphores[CurrentFrame];
    const vk::PipelineStageFlags pipelineStageFlags = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    submitInfo.pWaitDstStageMask = &pipelineStageFlags;

    VulkanUtils::CheckResult(
      VulkanContext::VulkanQueue.GraphicsQueue.submit(1, &submitInfo, InFlightFences[CurrentFrame]));
  }

  void VulkanSwapchain::SubmitPass(const std::function<void(VulkanCommandBuffer& commandBuffer)>& func) {
    const auto& LogicalDevice = VulkanContext::Context.Device;
    vk::ClearValue clearValues[1];
    clearValues[0].color = vk::ClearColorValue(std::array{0.0f, 0.0f, 0.0f, 1.0f});

    vk::RenderPassBeginInfo renderPassBeginInfo;
    renderPassBeginInfo.renderArea = vk::Rect2D{vk::Offset2D{}, Window::GetWindowExtent()};

    renderPassBeginInfo.clearValueCount = 1;
    renderPassBeginInfo.pClearValues = clearValues;

    ImagesInFlight[ImageIndex] = &InFlightFences[CurrentFrame];

    VulkanUtils::CheckResult(LogicalDevice.resetFences(1, &InFlightFences[CurrentFrame]));

    //Swapchain pass 
    auto& commandBuffer = GetCommandBuffer();
    renderPassBeginInfo.framebuffer = m_FrameBuffers[ImageIndex];
    renderPassBeginInfo.renderPass = m_RenderPass.Get();
    commandBuffer.Begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse});
    commandBuffer.BeginRenderPass(renderPassBeginInfo);
    commandBuffer.SetViwportWindow().SetScissorWindow();
    func(commandBuffer);
    commandBuffer.EndRenderPass();
    commandBuffer.End();
  }

  void VulkanSwapchain::Present() {
    ZoneScoped;
    vk::PresentInfoKHR presentInfo;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &RenderCompleteSemaphores[CurrentFrame];
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_SwapChain;
    presentInfo.pImageIndices = &ImageIndex;

    const auto& result = VulkanContext::VulkanQueue.PresentQueue.presentKHR(&presentInfo);

    IsOutOfDate = false;
    if (VulkanUtils::IsOutOfDate(result) || VulkanUtils::IsSubOptimal(result)) {
      if (VulkanUtils::IsOutOfDate(result)) {
        IsOutOfDate = true;
        RecreateSwapChain();
      }
    }

    CurrentFrame = (CurrentFrame + 1) % MaxFramesInFlight;
  }

  void VulkanSwapchain::ClearSwapChain() {
    const auto& LogicalDevice = VulkanContext::Context.Device;
    VulkanRenderer::WaitDeviceIdle();
    for (const auto& view : m_ImageViews) {
      LogicalDevice.destroyImageView(view);
    }
    m_ImageViews.clear();
    for (const auto& fb : m_FrameBuffers) {
      LogicalDevice.destroyFramebuffer(fb);
    }
    m_RenderPass.Destroy();
    InFlightFences.clear();
  }

  bool VulkanSwapchain::AcquireNextImage() {
    const auto& LogicalDevice = VulkanContext::Context.Device;

    const auto result = LogicalDevice.acquireNextImageKHR(m_SwapChain,
      UINT64_MAX,
      ImageAcquiredSemaphores[CurrentFrame]);

    if (VulkanUtils::IsOutOfDate(result.result) || VulkanUtils::IsSubOptimal(result.result)) {
      return false;
    }
    VulkanUtils::CheckResult(result.result);

    ImageIndex = result.value;
    return true;
  }

  void VulkanSwapchain::SetVsync(bool isVsync, bool recreate) {
    if (!isVsync)
      m_PresentMode = vk::PresentModeKHR::eImmediate;
    else
      m_PresentMode = vk::PresentModeKHR::eFifo;

    if (recreate)
      RecreateSwapChain();
  }

  bool VulkanSwapchain::ToggleVsync() {
    if (m_PresentMode == vk::PresentModeKHR::eFifo) {
      m_PresentMode = vk::PresentModeKHR::eImmediate;
      RecreateSwapChain();
      return false;
    }
    else if (m_PresentMode == vk::PresentModeKHR::eImmediate) {
      m_PresentMode = vk::PresentModeKHR::eFifo;
      RecreateSwapChain();
      return true;
    }
    return false;
  }

  VulkanCommandBuffer& VulkanSwapchain::GetCommandBuffer() {
    return CommandBuffers[ImageIndex];
  }
}
