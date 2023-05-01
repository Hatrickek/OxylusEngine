#pragma once
#include <vulkan/vulkan.hpp>

namespace Oxylus {
  struct RenderPassDescription {
    std::string Name = {};
    vk::RenderPassCreateInfo CreateInfo = {};
  };

  class VulkanRenderPass {
  public:
    VulkanRenderPass() = default;
    ~VulkanRenderPass();
    void CreateRenderPass(const vk::RenderPassCreateInfo& renderPassCI);
    void CreateRenderPass(const RenderPassDescription& description);
    void Destroy() const;

    const vk::RenderPass& Get() const { return m_RenderPass; }

    const RenderPassDescription& GetDescription() const { return m_Description; }

  private:
    RenderPassDescription m_Description;
    vk::RenderPass m_RenderPass;
  };
}
