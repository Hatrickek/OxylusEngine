#pragma once

#include "Utils/Log.h"

#include <tracy/Tracy.hpp>
#include <vulkan/vulkan.hpp>
#include <tracy/TracyVulkan.hpp>

#include <chrono>

#define GPU_PROFILER_ENABLED 1
#ifdef OX_DIST
#undef GPU_PROFILER_ENABLED
#define GPU_PROFILER_ENABLED 0
#endif

#if GPU_PROFILER_ENABLED
#define OX_TRACE_GPU(cmdbuf, name) TracyVkZone(Oxylus::TracyProfiler::GetContext(), cmdbuf, name)
#else
#define OX_TRACE_GPU(cmdbuf, name)
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
