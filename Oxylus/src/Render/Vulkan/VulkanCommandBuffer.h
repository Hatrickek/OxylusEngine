#pragma once
#include <vulkan/vulkan.hpp>

namespace Oxylus {
  class VulkanCommandBuffer {
  public:
    VulkanCommandBuffer() = default;

    void CreateBuffer(vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary);

    const VulkanCommandBuffer& Begin(const vk::CommandBufferBeginInfo& beginInfo) const;

    const VulkanCommandBuffer& BeginRenderPass(const vk::RenderPassBeginInfo& beginInfo,
                                               vk::SubpassContents subpassContents = vk::SubpassContents::eInline)
    const;

    void Dispatch(uint32_t x, uint32_t y, uint32_t z) const;

    const VulkanCommandBuffer& EndRenderPass() const;

    const VulkanCommandBuffer& End() const;

    VulkanCommandBuffer& PushConstants(vk::PipelineLayout layout,
                                       vk::ShaderStageFlags stageFlags,
                                       uint32_t offset,
                                       uint32_t size,
                                       const void* pValues);

    VulkanCommandBuffer& SetViwportWindow();
    VulkanCommandBuffer& SetFlippedViwportWindow();
    VulkanCommandBuffer& SetScissorWindow();
    VulkanCommandBuffer& SetViewport(vk::Viewport viewport);
    VulkanCommandBuffer& SetScissor(vk::Rect2D scissor);
    const vk::CommandBuffer& Get() const { return m_Buffer; }

    void FlushBuffer() const;

    void FreeBuffer() const;

  private:
    vk::CommandBuffer m_Buffer;
  };
}
