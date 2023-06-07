#pragma once

#include <tracy/Tracy.hpp>
#include <vulkan/vulkan.hpp>
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
#define OX_SCOPED_ZONE_N(name) OX_SCOPED_ZONE_N(name)
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
#define OX_ALLOC
#define OX_FREE
#endif

namespace Oxylus {
  class TracyProfiler {
  public:
    static void InitTracyForVulkan(VkPhysicalDevice physdev, VkDevice device, VkQueue queue, VkCommandBuffer cmdbuf);

    static void DestroyContext();

    static void Collect(const vk::CommandBuffer& commandBuffer);

    static TracyVkCtx& GetContext() { return s_VulkanContext; }

  private:
    static TracyVkCtx s_VulkanContext;
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
