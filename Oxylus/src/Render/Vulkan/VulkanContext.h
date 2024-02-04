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

namespace Ox {
class TracyProfiler;
struct AppSpec;

class VulkanContext {
public:
  VkDevice device = nullptr;
  VkPhysicalDevice physical_device = nullptr;
  VkQueue graphics_queue = nullptr;
  uint32_t graphics_queue_family_index = 0;
  VkQueue transfer_queue = nullptr;
  std::optional<vuk::Context> context;
  std::optional<vuk::DeviceSuperFrameResource> superframe_resource;
  std::optional<vuk::Allocator> superframe_allocator;
  bool has_rt = false;
  bool suspend = false;
  vuk::PresentModeKHR present_mode = vuk::PresentModeKHR::eFifo;
  vuk::SwapchainRef swapchain = nullptr;
  VkSurfaceKHR surface;
  vkb::Instance vkb_instance;
  vkb::Device vkb_device;
  uint32_t num_inflight_frames = 3;
  uint32_t num_frames = 0;
  uint32_t current_frame = 0;
  vuk::Unique<std::array<VkSemaphore, 3>> present_ready;
  vuk::Unique<std::array<VkSemaphore, 3>> render_complete;
  Shared<TracyProfiler> tracy_profiler = {};

  std::string device_name = {};
  uint32_t driver_version = {};

  VulkanContext() = default;

  static void init();

  void create_context(const AppSpec& spec);

  void rebuild_swapchain(vuk::PresentModeKHR new_present_mode = vuk::PresentModeKHR::eFifo);

  vuk::Allocator begin();
  void end(const vuk::Future& src, vuk::Allocator frame_allocator);

  static VulkanContext* get() { return s_instance; }

private:
  vuk::SingleSwapchainRenderBundle m_bundle = {};

  static VulkanContext* s_instance;
};
}
