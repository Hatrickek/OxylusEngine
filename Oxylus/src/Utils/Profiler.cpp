#include "Profiler.h"

#include "Log.h"
#include "Render/Vulkan/VulkanContext.h"
#include "Core/PlatformDetection.h"

namespace Oxylus {
TracyProfiler::~TracyProfiler() {
#if GPU_PROFILER_ENABLED
  destroy_context();
#endif
}

void TracyProfiler::init_tracy_for_vulkan(VulkanContext* context) {
#if GPU_PROFILER_ENABLED
  VkCommandPoolCreateInfo cpci{ .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT };
  cpci.queueFamilyIndex = context->graphics_queue_family_index;
  context->superframe_allocator->allocate_command_pools(std::span{ &*tracy_cpool, 1 }, std::span{ &cpci, 1 });
  vuk::CommandBufferAllocationCreateInfo ci{ .command_pool = *tracy_cpool };
  context->superframe_allocator->allocate_command_buffers(std::span{ &*tracy_cbufai, 1 }, std::span{ &ci, 1 });
  tracy_graphics_ctx = TracyVkContextCalibrated(
      context->vkb_instance.instance, context->physical_device, context->device, context->graphics_queue, tracy_cbufai->command_buffer, context->vkb_instance.fp_vkGetInstanceProcAddr, context->vkb_instance.fp_vkGetDeviceProcAddr);
  tracy_transfer_ctx = TracyVkContextCalibrated(
      context->vkb_instance.instance, context->physical_device, context->device, context->graphics_queue, tracy_cbufai->command_buffer, context->vkb_instance.fp_vkGetInstanceProcAddr, context->vkb_instance.fp_vkGetDeviceProcAddr);
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

ProfilerTimer::ProfilerTimer(const char* name) : m_Name(name), m_ElapsedTime(0), m_Stopped(false) {
#ifndef OX_PLATFORM_LINUX
  m_StartTimepoint = std::chrono::steady_clock::now();
#endif
}

ProfilerTimer::~ProfilerTimer() {
#ifndef OX_PLATFORM_LINUX
  if (!m_Stopped)
    Stop();
#endif
}

double ProfilerTimer::ElapsedMilliSeconds() const {
  return static_cast<double>(m_ElapsedTime.count()) / 1000.0;
}

double ProfilerTimer::ElapsedMicroSeconds() const {
  return static_cast<double>(m_ElapsedTime.count());
}

void ProfilerTimer::Stop() {
#ifndef OX_PLATFORM_LINUX
  const auto endTimepoint = std::chrono::steady_clock::now();
  m_ElapsedTime = std::chrono::time_point_cast<std::chrono::microseconds>(endTimepoint).time_since_epoch() -
                  std::chrono::time_point_cast<std::chrono::microseconds>(m_StartTimepoint).time_since_epoch();

  m_Stopped = true;
#endif
}

void ProfilerTimer::Reset() {
#ifndef OX_PLATFORM_LINUX
  m_StartTimepoint = std::chrono::high_resolution_clock::now();
#endif
}

void ProfilerTimer::Print(const std::string_view arg) {
#ifndef OX_PLATFORM_LINUX
  Stop();
  if (arg.empty())
    OX_CORE_TRACE("{0} {1} ms", m_Name, ElapsedMilliSeconds());
  else
    OX_CORE_TRACE("{0} {1} ms", arg, ElapsedMilliSeconds());
#endif
}
}
