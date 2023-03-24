#pragma once
#include "Vulkan/VulkanCommandBuffer.h"
#include "Vulkan/VulkanDescriptorSet.h"
#include "Vulkan/VulkanFramebuffer.h"
#include "Vulkan/VulkanPipeline.h"
#include "Vulkan/VulkanSwapchain.h"
#include "Vulkan/Utils/PipelineStats.h"

namespace Oxylus {
  class RenderGraph;

  struct SwapchainPass {
    VulkanDescriptorSet* Desc;
  };

  struct RenderGraphPass {
    std::string Name;
    std::vector<VulkanCommandBuffer*> CommandBuffers;
    std::vector<VulkanFramebuffer*> Framebuffers;
    std::function<void(VulkanCommandBuffer& commandBuffer, int32_t framebufferIndex)> Execute;
    std::array<vk::ClearValue, 2> ClearValues;
    vk::Queue* SubmitQueue;
    VulkanPipeline* Pipeline;

    RenderGraphPass(std::string name,
                    std::vector<VulkanCommandBuffer*> commandBuffers,
                    VulkanPipeline* pipeline,
                    std::vector<VulkanFramebuffer*> framebuffers,
                    std::function<void(VulkanCommandBuffer& commandBuffer, int32_t framebufferIndex)> execute,
                    std::array<vk::ClearValue, 2> clearValues = {},
                    vk::Queue* submitQueue = {}) : Name(std::move(name)), CommandBuffers(std::move(commandBuffers)),
                                                   Framebuffers(std::move(framebuffers)), Execute(std::move(execute)),
                                                   ClearValues(clearValues), SubmitQueue(submitQueue),
                                                   Pipeline(pipeline) {
      Init();
    }

    ~RenderGraphPass() = default;

    RenderGraphPass& AddInnerPass(const RenderGraphPass& innerPass);
    RenderGraphPass& AddReadDependency(const RenderGraph& renderGraph, const std::string& passName);
    RenderGraphPass& SetRenderArea(const vk::Rect2D& renderArea);
    RenderGraphPass& AddToGraph(RenderGraph& renderGraph);
    RenderGraphPass& AddToGraphCompute(RenderGraph& renderGraph);
    RenderGraphPass& RunWithCondition(bool& condition);

  private:
    void Init();
    std::vector<RenderGraphPass> m_InnerPasses{};

    vk::Fence m_Fence;
    vk::Semaphore m_SignalSemaphore{};
    std::vector<vk::Semaphore> m_WaitSemaphores{};

    bool* m_RunCondition = nullptr;
    bool m_IsComputePass = false;
    mutable bool m_HasDependency = false;

    vk::Rect2D m_RenderArea{};

    friend RenderGraph;
  };

  class RenderGraph {
  public:
    RenderGraph() {
      m_RenderGraphPasses.reserve(10);
    }

    ~RenderGraph() = default;

    RenderGraph& AddRenderPass(RenderGraphPass& renderGraphPass);
    RenderGraph& AddComputePass(RenderGraphPass& computePass);
    void RemoveRenderPass(const std::string& Name);

    RenderGraph& SetSwapchain(const SwapchainPass& swapchainPass);

    const RenderGraphPass* FindRenderGraphPass(const std::string& name) const;

    bool Update(VulkanSwapchain& swapchain, const uint32_t* currentFrame);

  private:
    std::unordered_map<std::string, RenderGraphPass> m_RenderGraphPasses;
    SwapchainPass m_SwapchainPass;
  };
}
