#pragma once

#include <tracy/Tracy.hpp>
#include "vulkan/vulkan.h"
#include <tracy/TracyVulkan.hpp>

#include <chrono>

// Profilers 
#define GPU_PROFILER_ENABLED 0
#define CPU_PROFILER_ENABLED 0
#define MEMORY_PROFILER_ENABLED 0

#ifdef OX_DISTRIBUTION
#undef GPU_PROFILER_ENABLED
#define GPU_PROFILER_ENABLED 0
#endif

#if GPU_PROFILER_ENABLED
#define OX_TRACE_GPU(cmdbuf, name) TracyVkZone(Oxylus::TracyProfiler::GetContext(), cmdbuf, name)
#else
#define OX_TRACE_GPU(cmdbuf, name)
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
  static void InitTracyForVulkan(const VulkanContext* context, VkCommandBuffer command_buffer);

  static void DestroyContext();

  static void Collect(const VkCommandBuffer& commandBuffer);

  static TracyVkCtx& GetContext() { return vulkan_context; }

private:
  static TracyVkCtx vulkan_context;
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

  void Print(const std::string_view arg = "");

private:
  const char* m_Name;
  std::chrono::microseconds m_ElapsedTime;
  std::chrono::time_point<std::chrono::steady_clock> m_StartTimepoint;
  bool m_Stopped;
};
}
