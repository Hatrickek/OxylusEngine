#include "src/oxpch.h"
#include "RenderGraph.h"
#include "Utils/Log.h"
#include "Vulkan/VulkanContext.h"
#include "Vulkan/VulkanRenderer.h"
#include "Utils/Profiler.h"

#include "Vulkan/Utils/VulkanUtils.h"

namespace Oxylus {
  const RenderGraphPass* RenderGraph::FindRenderGraphPass(const std::string& name) const {
    const RenderGraphPass* renderGraphPass = nullptr;

    for (auto& [fst, snd] : m_RenderGraphPasses) {
      if (fst == name) {
        renderGraphPass = &snd;
      }
    }

    return renderGraphPass;
  }

  RenderGraphPass& RenderGraphPass::AddInnerPass(const RenderGraphPass& innerPass) {
    m_InnerPasses.emplace_back(innerPass);
    return *this;
  }

  RenderGraphPass& RenderGraphPass::AddReadDependency(const RenderGraph& renderGraph, const std::string& passName) {
    const auto& pass = renderGraph.FindRenderGraphPass(passName);
    if (!pass) {
      OX_CORE_BERROR("Can't find {0} named render pass to add as dependency!", passName);
      return *this;
    }
    m_WaitSemaphores.emplace_back(pass->m_SignalSemaphore);
    pass->m_HasDependency = true;
    return *this;
  }

  RenderGraphPass& RenderGraphPass::SetRenderArea(const vk::Rect2D& renderArea) {
    m_RenderArea = renderArea;
    return *this;
  }

  RenderGraphPass& RenderGraphPass::AddToGraph(RenderGraph& renderGraph) {
    renderGraph.AddRenderPass(*this);
    return *this;
  }

  RenderGraphPass& RenderGraphPass::AddToGraphCompute(RenderGraph& renderGraph) {
    renderGraph.AddComputePass(*this);
    return *this;
  }

  RenderGraphPass& RenderGraphPass::RunWithCondition(bool& condition) {
    m_RunCondition = &condition;
    return *this;
  }

  void RenderGraphPass::Init() {
    const auto& LogicalDevice = VulkanContext::Context.Device;
    constexpr vk::FenceCreateInfo fenceCreateInfo{};
    VulkanUtils::CheckResult(LogicalDevice.createFence(&fenceCreateInfo, nullptr, &m_Fence));

    constexpr vk::SemaphoreCreateInfo semaphoreCreateInfo;
    VulkanUtils::CheckResult(LogicalDevice.createSemaphore(&semaphoreCreateInfo, nullptr, &m_SignalSemaphore));
  }

  RenderGraph& RenderGraph::AddRenderPass(RenderGraphPass& renderGraphPass) {
    if (FindRenderGraphPass(renderGraphPass.Name)) {
      OX_CORE_BERROR("There can't be two render passes with the same name!");
      return *this;
    }
    m_RenderGraphPasses.emplace(renderGraphPass.Name, renderGraphPass);
    return *this;
  }

  RenderGraph& RenderGraph::AddComputePass(RenderGraphPass& computePass) {
    if (FindRenderGraphPass(computePass.Name)) {
      OX_CORE_BERROR("There can't be two compute passes with the same name!");
    }
    computePass.m_IsComputePass = true;
    m_RenderGraphPasses.emplace(computePass.Name, computePass);
    return *this;
  }

  void RenderGraph::RemoveRenderPass(const std::string& Name) {
    if (!FindRenderGraphPass(Name)) {
      OX_CORE_BERROR("Can't find {0} named render pass to remove!", Name);
      return;
    }
    m_RenderGraphPasses.erase(Name);
  }

  RenderGraph& RenderGraph::SetSwapchain(const SwapchainPass& swapchainPass) {
    m_SwapchainPass = swapchainPass;
    return *this;
  }

  bool RenderGraph::Update(VulkanSwapchain& swapchain, const uint32_t* currentFrame) {
    const auto& LogicalDevice = VulkanContext::GetDevice();

    static bool isFirstPass = true;

    VulkanUtils::CheckResult(LogicalDevice.waitForFences(1, &swapchain.InFlightFences[*currentFrame], true,UINT64_MAX));
    VulkanUtils::CheckResult(LogicalDevice.resetFences(1, &swapchain.InFlightFences[*currentFrame]));

    if (swapchain.Resizing || !swapchain.AcquireNextImage() && !isFirstPass) {
      VulkanRenderer::ResizeBuffers();
      return false;
    }

    for (auto& [name, renderPass] : m_RenderGraphPasses) {
      if (renderPass.m_RunCondition != nullptr && !renderPass.m_RunCondition)
        continue;
      ZoneScoped;
      //Render pass
      if (!isFirstPass) {
        VulkanUtils::CheckResult(LogicalDevice.waitForFences(1, &renderPass.m_Fence, true, UINT64_MAX));
        VulkanUtils::CheckResult(LogicalDevice.resetFences(1, &renderPass.m_Fence));
      }
      const auto& commandBuffer = renderPass.CommandBuffers[0];
      commandBuffer->Begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse});
      if (!renderPass.m_IsComputePass) {
        vk::RenderPassBeginInfo beginInfo;
        if (renderPass.m_RenderArea.extent.height < 1) {
          beginInfo.renderArea = vk::Rect2D{vk::Offset2D{}, Window::GetWindowExtent()};
        }
        else {
          beginInfo.renderArea = renderPass.m_RenderArea;
        }
        beginInfo.renderPass = renderPass.Pipeline->GetRenderPass().Get();
        beginInfo.clearValueCount = (uint32_t)renderPass.ClearValues.size();
        beginInfo.pClearValues = renderPass.ClearValues.data();

        for (int32_t i = 0; i < (int32_t)renderPass.Framebuffers.size(); i++) {
          beginInfo.framebuffer = renderPass.Framebuffers[i]->Get();
          commandBuffer->BeginRenderPass(beginInfo);
          renderPass.Execute(*commandBuffer, i);
          commandBuffer->EndRenderPass();
        }
        for (const auto& innerPass : renderPass.m_InnerPasses) {
          beginInfo.renderPass = innerPass.Pipeline->GetRenderPass().Get();
          for (int32_t i = 0; i < (int32_t)innerPass.Framebuffers.size(); i++) {
            beginInfo.framebuffer = innerPass.Framebuffers[i]->Get();
            commandBuffer->BeginRenderPass(beginInfo);
            innerPass.Execute(*commandBuffer, i);
            commandBuffer->EndRenderPass();
          }
        }

        TracyProfiler::Collect(commandBuffer->Get());

        commandBuffer->End();
      }
      else {
        renderPass.Execute(*commandBuffer, 0);

        TracyProfiler::Collect(commandBuffer->Get());

        commandBuffer->End();
      }

      //Submit
      vk::SubmitInfo submitInfo = {};
      submitInfo.commandBufferCount = 1;
      submitInfo.pCommandBuffers = &commandBuffer->Get();
      if (renderPass.m_HasDependency) {
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &renderPass.m_SignalSemaphore;
      }
      submitInfo.waitSemaphoreCount = (uint32_t)renderPass.m_WaitSemaphores.size();
      const std::vector<vk::PipelineStageFlags> stageColorFlag = {vk::PipelineStageFlagBits::eColorAttachmentOutput};

      if (!renderPass.m_WaitSemaphores.empty()) {
        submitInfo.pWaitSemaphores = renderPass.m_WaitSemaphores.data();
        submitInfo.pWaitDstStageMask = stageColorFlag.data();
      }

      VulkanUtils::CheckResult(renderPass.SubmitQueue->submit(1, &submitInfo, renderPass.m_Fence));
    }

    isFirstPass = false;

    return true;
  }
}
