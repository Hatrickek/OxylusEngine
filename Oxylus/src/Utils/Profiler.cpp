#include "src/oxpch.h"
#include "Profiler.h"

#include "Render/Vulkan/VulkanContext.h"

namespace Oxylus {
#ifndef OX_DIST
  TracyVkCtx TracyProfiler::s_VulkanContext;
#endif

  void TracyProfiler::InitTracyForVulkan(VkPhysicalDevice physdev,
                                         VkDevice device,
                                         VkQueue queue,
                                         VkCommandBuffer cmdbuf) {
#ifndef OX_DIST
    const auto timedomains = (PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT)vkGetDeviceProcAddr(
            VulkanContext::Context.Device,
            "vkGetPhysicalDeviceCalibrateableTimeDomainsEXT");

    const auto timesteps = (PFN_vkGetCalibratedTimestampsEXT)vkGetDeviceProcAddr(VulkanContext::Context.Device,
                                                                                 "vkGetCalibratedTimestampsEXT");

    s_VulkanContext = TracyVkContextCalibrated(physdev, device, queue, cmdbuf, timedomains, timesteps);
#endif
  }

  void TracyProfiler::DestroyContext() {
#ifndef OX_DIST
    TracyVkDestroy(s_VulkanContext);
#endif
  }

  void TracyProfiler::Collect(const vk::CommandBuffer& commandBuffer) {
#if !defined(OX_DIST) 
//    s_VulkanContext->Collect(commandBuffer);
#endif
  }

  ProfilerTimer::ProfilerTimer(const char* name) : m_Name(name), m_ElapsedTime(0), m_Stopped(false) {
    m_StartTimepoint = std::chrono::steady_clock::now();
  }

  ProfilerTimer::~ProfilerTimer() {
    if (!m_Stopped)
      Stop();
  }

  double ProfilerTimer::ElapsedMilliSeconds() const {
    return static_cast<double>(m_ElapsedTime.count()) / 1000.0;
  }

  double ProfilerTimer::ElapsedMicroSeconds() const {
    return static_cast<double>(m_ElapsedTime.count());
  }

  void ProfilerTimer::Stop() {
    const auto endTimepoint = std::chrono::steady_clock::now();
    m_ElapsedTime = std::chrono::time_point_cast<std::chrono::microseconds>(endTimepoint).time_since_epoch() -
                    std::chrono::time_point_cast<std::chrono::microseconds>(m_StartTimepoint).time_since_epoch();

    m_Stopped = true;
  }

  void ProfilerTimer::Reset() {
    m_StartTimepoint = std::chrono::high_resolution_clock::now();
  }

  void ProfilerTimer::Print(const std::string_view arg) {
    Stop();
    if (arg.empty())
      OX_CORE_TRACE("{0} {1} ms", m_Name, ElapsedMilliSeconds());
    else
      OX_CORE_TRACE("{0} {1} ms", arg, ElapsedMilliSeconds());
  }
}
