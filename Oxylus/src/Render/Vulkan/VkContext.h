#pragma once

#include "Core/Base.h"

#include <optional>

#include <VkBootstrap.h>
#include <vuk/Allocator.hpp>
#include <vuk/AllocatorHelpers.hpp>
#include <vuk/Context.hpp>
#include <vuk/Types.hpp>
#include <vuk/resources/DeviceFrameResource.hpp>


namespace vkb {
struct Device;
struct Instance;
}

namespace ox {
class TracyProfiler;
struct AppSpec;

class VkContext {
public:
  VkDevice device = nullptr;
  VkPhysicalDevice physical_device = nullptr;
  vkb::PhysicalDevice vkbphysical_device;
  VkQueue graphics_queue = nullptr;
  uint32_t graphics_queue_family_index = 0;
  VkQueue transfer_queue = nullptr;
  std::optional<vuk::Context> context;
  std::optional<vuk::DeviceSuperFrameResource> superframe_resource;
  std::optional<vuk::Allocator> superframe_allocator;
  bool suspend = false;
  vuk::PresentModeKHR present_mode = vuk::PresentModeKHR::eFifo;
  vuk::SwapchainRef swapchain = nullptr;
  VkSurfaceKHR surface;
  vkb::Instance vkb_instance;
  vkb::Device vkb_device;
  uint32_t num_inflight_frames = 3;
  uint64_t num_frames = 0;
  uint32_t current_frame = 0;
  vuk::Unique<std::array<VkSemaphore, 3>> present_ready;
  vuk::Unique<std::array<VkSemaphore, 3>> render_complete;
  Shared<TracyProfiler> tracy_profiler = {};

  std::string device_name = {};

  VkContext() = default;

  static void init();
  static VkContext* get() { return s_instance; }

  void create_context(const AppSpec& spec);

  void rebuild_swapchain(vuk::PresentModeKHR new_present_mode = vuk::PresentModeKHR::eFifo);

  vuk::Allocator begin();
  void end(const vuk::Future& src, vuk::Allocator frame_allocator);

  uint32_t get_max_viewport_count() const { return vkbphysical_device.properties.limits.maxViewports; }

private:
  vuk::SingleSwapchainRenderBundle m_bundle = {};

  static VkContext* s_instance;
};
}
