#pragma once
#include <vulkan/vulkan.hpp>

namespace Oxylus {
  class VulkanRenderPass {
  public:
    VulkanRenderPass() = default;
    ~VulkanRenderPass();
    void CreateRenderPass(const vk::RenderPassCreateInfo& renderPassCI);
    void Destroy() const;

    const vk::RenderPass& Get() const {
      return m_RenderPass;
    }

    const vk::RenderPassCreateInfo& GetDesc() const {
      return m_Description;
    }

  private:
    vk::RenderPass m_RenderPass;
    vk::RenderPassCreateInfo m_Description;
  };
}
