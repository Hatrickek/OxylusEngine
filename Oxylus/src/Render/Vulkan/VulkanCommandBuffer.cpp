#include "src/oxpch.h"
#include "VulkanCommandBuffer.h"
#include "VulkanContext.h"
#include "VulkanRenderer.h"
#include "Utils/VulkanUtils.h"

namespace Oxylus {
  void VulkanCommandBuffer::CreateBuffer(vk::CommandBufferLevel level) {
    vk::CommandBufferAllocateInfo cmdBufAllocateInfo;
    cmdBufAllocateInfo.commandPool = VulkanRenderer::s_RendererContext.CommandPool;
    cmdBufAllocateInfo.level = level;
    cmdBufAllocateInfo.commandBufferCount = 1;
    m_Buffer = VulkanContext::GetDevice().allocateCommandBuffers(cmdBufAllocateInfo).value[0];
  }

  const VulkanCommandBuffer& VulkanCommandBuffer::Begin(const vk::CommandBufferBeginInfo& beginInfo) const {
    OX_CORE_ASSERT(m_Buffer);
    VulkanUtils::CheckResult(m_Buffer.begin(beginInfo));
    return *this;
  }

  const VulkanCommandBuffer& VulkanCommandBuffer::BeginRenderPass(const vk::RenderPassBeginInfo& beginInfo,
                                                                  vk::SubpassContents subpassContents) const {
    OX_CORE_ASSERT(m_Buffer);
    m_Buffer.beginRenderPass(beginInfo, subpassContents);
    return *this;
  }

  void VulkanCommandBuffer::Dispatch(uint32_t x, uint32_t y, uint32_t z) const {
    m_Buffer.dispatch(x, y, z);
  }

  const VulkanCommandBuffer& VulkanCommandBuffer::EndRenderPass() const {
    m_Buffer.endRenderPass();
    return *this;
  }

  const VulkanCommandBuffer& VulkanCommandBuffer::End() const {
    VulkanUtils::CheckResult(m_Buffer.end());
    return *this;
  }

  VulkanCommandBuffer& VulkanCommandBuffer::PushConstants(vk::PipelineLayout layout,
                                                          vk::ShaderStageFlags stageFlags,
                                                          uint32_t offset,
                                                          uint32_t size,
                                                          const void* pValues) {
    m_Buffer.pushConstants(layout, stageFlags, offset, size, pValues);
    return *this;
  }

  VulkanCommandBuffer& VulkanCommandBuffer::SetViwportWindow() {
    const vk::Viewport viewport{0.0f, 0.0f, (float)Window::GetWidth(), (float)Window::GetHeight(), 0.0f, 1.0f,};
    m_Buffer.setViewport(0, 1, &viewport);
    return *this;
  }

  VulkanCommandBuffer& VulkanCommandBuffer::SetFlippedViwportWindow() {
    vk::Viewport flippedViewport;
    flippedViewport.x = 0;
    flippedViewport.y = (float)Window::GetHeight();
    flippedViewport.minDepth = 0;
    flippedViewport.maxDepth = 1;
    flippedViewport.width = (float)Window::GetWidth();
    flippedViewport.height = -(float)Window::GetHeight();
    m_Buffer.setViewport(0, 1, &flippedViewport);
    return *this;
  }

  VulkanCommandBuffer& VulkanCommandBuffer::SetScissorWindow() {
    const vk::Rect2D scissor{vk::Offset2D{}, Window::GetWindowExtent()};

    m_Buffer.setScissor(0, 1, &scissor);
    return *this;
  }

  VulkanCommandBuffer& VulkanCommandBuffer::SetViewport(vk::Viewport viewport) {
    m_Buffer.setViewport(0, 1, &viewport);
    return *this;
  }

  VulkanCommandBuffer& VulkanCommandBuffer::SetScissor(vk::Rect2D scissor) {
    m_Buffer.setScissor(0, 1, &scissor);
    return *this;
  }

  void VulkanCommandBuffer::FlushBuffer() const {
    VulkanUtils::CheckResult(VulkanContext::VulkanQueue.GraphicsQueue.submit(vk::SubmitInfo{0, nullptr, nullptr, 1, &m_Buffer}, vk::Fence()));
    VulkanUtils::CheckResult(VulkanContext::VulkanQueue.GraphicsQueue.waitIdle());
  }

  void VulkanCommandBuffer::FreeBuffer() const {
    VulkanContext::Context.Device.freeCommandBuffers(VulkanRenderer::s_RendererContext.CommandPool, this->m_Buffer);
  }
}
