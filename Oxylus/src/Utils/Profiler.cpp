#include "Profiler.h"

#include "Render/Vulkan/VulkanContext.h"
#include "Core/PlatformDetection.h"

namespace Oxylus {
  TracyVkCtx TracyProfiler::s_VulkanContext;

  void TracyProfiler::InitTracyForVulkan(VkPhysicalDevice physdev,
                                         VkDevice device,
                                         VkQueue queue,
                                         VkCommandBuffer cmdbuf) {
#if GPU_PROFILER_ENABLED
    const auto timedomains = (PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT)vkGetDeviceProcAddr(
            VulkanContext::Context.Device,
            "vkGetPhysicalDeviceCalibrateableTimeDomainsEXT");

    const auto timesteps = (PFN_vkGetCalibratedTimestampsEXT)vkGetDeviceProcAddr(VulkanContext::Context.Device,
                                                                                 "vkGetCalibratedTimestampsEXT");

    s_VulkanContext = TracyVkContextCalibrated(physdev, device, queue, cmdbuf, timedomains, timesteps);
#endif
  }

  void TracyProfiler::DestroyContext() {
#if GPU_PROFILER_ENABLED
    TracyVkDestroy(s_VulkanContext);
#endif
  }

  void TracyProfiler::Collect(const vk::CommandBuffer& commandBuffer) {
#if GPU_PROFILER_ENABLED
    s_VulkanContext->Collect(commandBuffer);
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
