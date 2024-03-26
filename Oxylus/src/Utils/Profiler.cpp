#include "Profiler.hpp"

#include "Core/PlatformDetection.hpp"
#include "Render/Vulkan/VkContext.hpp"
#include "Log.hpp"

namespace ox {
#ifdef TRACY_ENABLE
TracyProfiler::~TracyProfiler() {
#if GPU_PROFILER_ENABLED
  destroy_context();
#endif
}

void TracyProfiler::init_tracy_for_vulkan(VkContext* context) {
#if GPU_PROFILER_ENABLED
  VkCommandPoolCreateInfo cpci{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT};
  cpci.queueFamilyIndex = context->graphics_queue_family_index;
  context->superframe_allocator->allocate_command_pools(std::span{&*tracy_cpool, 1}, std::span{&cpci, 1});
  vuk::CommandBufferAllocationCreateInfo ci{.command_pool = *tracy_cpool};
  context->superframe_allocator->allocate_command_buffers(std::span{&*tracy_cbufai, 1}, std::span{&ci, 1});
  tracy_graphics_ctx = TracyVkContextCalibrated(
    context->vkb_instance.instance, context->physical_device, context->device, context->graphics_queue, tracy_cbufai->command_buffer, context->vkb_instance.fp_vkGetInstanceProcAddr, context->vkb_instance.fp_vkGetDeviceProcAddr);
  tracy_transfer_ctx = TracyVkContextCalibrated(
    context->vkb_instance.instance, context->physical_device, context->device, context->graphics_queue, tracy_cbufai->command_buffer, context->vkb_instance.fp_vkGetInstanceProcAddr, context->vkb_instance.fp_vkGetDeviceProcAddr);
  OX_LOG_INFO("Tracy profiler initialized.");
#endif
}

void TracyProfiler::destroy_context() const {
#if GPU_PROFILER_ENABLED
  TracyVkDestroy(tracy_graphics_ctx)
  TracyVkDestroy(tracy_transfer_ctx)
#endif
}

void TracyProfiler::collect(tracy::VkCtx* ctx, const VkCommandBuffer& command_buffer) {
#if GPU_PROFILER_ENABLED
  ctx->Collect(command_buffer);
#endif
}

#endif
}
