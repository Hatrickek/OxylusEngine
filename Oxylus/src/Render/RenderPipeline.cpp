#include "RenderPipeline.h"
#include <vuk/Future.hpp>
#include <vuk/RenderGraph.hpp>

#include "Vulkan/VulkanContext.h"

namespace Oxylus {
void RenderPipeline::enqueue_future(vuk::Future&& fut) {
  std::scoped_lock _(setup_lock);
  futures.emplace_back(std::move(fut));
}

void RenderPipeline::wait_for_futures(vuk::Allocator& allocator) {
  vuk::Compiler compiler;
  wait_for_futures_explicit(allocator, compiler, futures);
  futures.clear();
}

void RenderPipeline::detach_swapchain(const vuk::Dimension3D dim) {
  attach_swapchain = false;
  dimension = dim;
}
}
