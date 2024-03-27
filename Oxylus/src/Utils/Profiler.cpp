#include "Profiler.hpp"

#include "Core/PlatformDetection.hpp"
#include "Log.hpp"
#include "Render/Vulkan/VkContext.hpp"

namespace ox {
TracyProfiler::~TracyProfiler() { destroy_context(); }

void TracyProfiler::init_tracy_for_vulkan(VkContext* context) {
#ifndef GPU_PROFILER_ENABLED
  return;
#endif
  VkCommandPoolCreateInfo cpci{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT};
  cpci.queueFamilyIndex = context->graphics_queue_family_index;
  context->superframe_allocator->allocate_command_pools(std::span{&*tracy_cpool, 1}, std::span{&cpci, 1});
  vuk::CommandBufferAllocationCreateInfo ci{.command_pool = *tracy_cpool};
  context->superframe_allocator->allocate_command_buffers(std::span{&*tracy_cbufai, 1}, std::span{&ci, 1});
  tracy_graphics_ctx = TracyVkContextCalibrated(context->vkb_instance.instance,
                                                context->physical_device,
                                                context->device,
                                                context->graphics_queue,
                                                tracy_cbufai->command_buffer,
                                                context->vkb_instance.fp_vkGetInstanceProcAddr,
                                                context->vkb_instance.fp_vkGetDeviceProcAddr);
  tracy_transfer_ctx = TracyVkContextCalibrated(context->vkb_instance.instance,
                                                context->physical_device,
                                                context->device,
                                                context->graphics_queue,
                                                tracy_cbufai->command_buffer,
                                                context->vkb_instance.fp_vkGetInstanceProcAddr,
                                                context->vkb_instance.fp_vkGetDeviceProcAddr);
  OX_LOG_INFO("Tracy GPU profiler initialized.");
}
vuk::ProfilingCallbacks TracyProfiler::setup_vuk_callback() {
#ifndef GPU_PROFILER_ENABLED
  return vuk::ProfilingCallbacks{};
#endif
  vuk::ProfilingCallbacks cbs;
  cbs.user_data = this;
  cbs.on_begin_command_buffer = [](void* user_data, VkCommandBuffer cbuf) {
    const auto* tracy_profiler = reinterpret_cast<TracyProfiler*>(user_data);
    TracyVkCollect(tracy_profiler->get_graphics_ctx(), cbuf);
    TracyVkCollect(tracy_profiler->get_transfer_ctx(), cbuf);
    return (void*)nullptr;
  };
  // runs whenever entering a new vuk::Pass
  // we start a GPU zone and then keep it open
  cbs.on_begin_pass = [](void* user_data, vuk::Name pass_name, VkCommandBuffer cmdbuf, vuk::DomainFlagBits domain) {
    const auto* tracy_profiler = reinterpret_cast<TracyProfiler*>(user_data);
    void* pass_data = new char[sizeof(tracy::VkCtxScope)];
    if (domain & vuk::DomainFlagBits::eGraphicsQueue) {
      new (pass_data) TracyVkZoneTransient(tracy_profiler->get_graphics_ctx(), , cmdbuf, pass_name.c_str(), true);
    } else if (domain & vuk::DomainFlagBits::eTransferQueue) {
      new (pass_data) TracyVkZoneTransient(tracy_profiler->get_transfer_ctx(), , cmdbuf, pass_name.c_str(), true);
    }

    return pass_data;
  };
  // runs whenever a pass has ended, we end the GPU zone we started
  cbs.on_end_pass = [](void* user_data, void* pass_data) {
    const auto tracy_scope = reinterpret_cast<tracy::VkCtxScope*>(pass_data);
    tracy_scope->~VkCtxScope();
  };

  return cbs;
}

void TracyProfiler::destroy_context() const {
#if GPU_PROFILER_ENABLED
  TracyVkDestroy(tracy_graphics_ctx) TracyVkDestroy(tracy_transfer_ctx)
#endif
}
} // namespace ox
