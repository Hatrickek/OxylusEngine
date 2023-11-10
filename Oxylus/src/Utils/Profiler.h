#pragma once

#include <vuk/Types.hpp>

// Profilers 
#define GPU_PROFILER_ENABLED 0
#define CPU_PROFILER_ENABLED 0
#define MEMORY_PROFILER_ENABLED 0

#define TRACY_VK_USE_SYMBOL_TABLE

#include <tracy/Tracy.hpp>
#include <vulkan/vulkan.h>
#include <tracy/TracyVulkan.hpp>

#ifdef OX_DISTRIBUTION
#undef GPU_PROFILER_ENABLED
#define GPU_PROFILER_ENABLED 0
#endif

#if GPU_PROFILER_ENABLED
#define OX_TRACE_GPU_TRANSIENT(context, cmdbuf, name) TracyVkZoneTransient(context, , cmdbuf, name, true) 
#else
#define OX_TRACE_GPU_TRANSIENT(context, cmdbuf, name)
#endif

#ifdef OX_DISTRIBUTION
#undef CPU_PROFILER_ENABLED
#define CPU_PROFILER_ENABLED 0
#endif

#if CPU_PROFILER_ENABLED
#define OX_SCOPED_ZONE ZoneScoped
#define OX_SCOPED_ZONE_N(name) ZoneScopedN(name)
#else
#define OX_SCOPED_ZONE
#define OX_SCOPED_ZONE_N(name) 
#endif

#ifdef OX_DISTRIBUTION
#undef MEMORY_PROFILER_ENABLED
#define MEMORY_PROFILER_ENABLED 0
#endif

#if MEMORY_PROFILER_ENABLED
#define OX_ALLOC(ptr, size) TracyAlloc(ptr, size)
#define OX_FREE(ptr) TracyFree(ptr)
#else
#define OX_ALLOC(ptr, size)
#define OX_FREE(ptr)
#endif

namespace Oxylus {
class VulkanContext;

class TracyProfiler {
public:

  TracyProfiler() = default;
  ~TracyProfiler();

  void init_tracy_for_vulkan(VulkanContext* context);
  void destroy_context() const;
  void collect(tracy::VkCtx* ctx, const VkCommandBuffer& command_buffer);

  tracy::VkCtx* get_graphics_ctx() const { return tracy_graphics_ctx; }
  tracy::VkCtx* get_transfer_ctx() const { return tracy_transfer_ctx; }

private:
  tracy::VkCtx* tracy_graphics_ctx;
  tracy::VkCtx* tracy_transfer_ctx;
  vuk::Unique<vuk::CommandPool> tracy_cpool;
  vuk::Unique<vuk::CommandBufferAllocation> tracy_cbufai;
};

using FloatingPointMicroseconds = std::chrono::duration<double, std::micro>;

class ProfilerTimer {
public:
  ProfilerTimer(const char* name = "");

  ~ProfilerTimer();

  double ElapsedMilliSeconds() const;

  double ElapsedMicroSeconds() const;

  void Stop();

  void Reset();

  void Print(std::string_view arg = "");

private:
  const char* m_Name;
  bool m_Stopped;
};
}
