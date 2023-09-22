#include "RenderPipeline.h"
#include <vuk/Future.hpp>
#include <vuk/RenderGraph.hpp>

#include "Vulkan/VulkanContext.h"

namespace Oxylus {
void RenderPipeline::EnqueueFuture(vuk::Future&& fut) {
  std::scoped_lock _(m_SetupLock);
  m_Futures.emplace_back(std::move(fut));
}

void RenderPipeline::WaitForFutures(VulkanContext* vkContext) {
  vuk::Compiler compiler;
  wait_for_futures_explicit(*vkContext->SuperframeAllocator, compiler, m_Futures);
  m_Futures.clear();
}

void RenderPipeline::DetachSwapchain(vuk::Dimension3D dimension) {
  m_AttachSwapchain = false;
  m_Dimension = dimension;
}
}
