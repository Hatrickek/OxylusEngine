#pragma once
#include <optional>

#include <vuk/Types.hpp>
#include <vuk/Context.hpp>
#include <vuk/resources/DeviceFrameResource.hpp>
#include <vuk/Allocator.hpp>
#include <vuk/AllocatorHelpers.hpp>
#include <VkBootstrap.h>
#include <vuk/Future.hpp>

namespace vkb {
struct Device;
struct Instance;
}

namespace Oxylus {
struct AppSpec;

class VulkanContext {
public:
  VkDevice device = nullptr;
  VkPhysicalDevice physical_device = nullptr;
  VkQueue graphics_queue = nullptr;
  VkQueue transfer_queue = nullptr;
  std::optional<vuk::Context> context;
  std::optional<vuk::DeviceSuperFrameResource> superframe_resource;
  std::optional<vuk::Allocator> superframe_allocator;
  bool has_rt = false;
  bool suspend = false;
  vuk::SwapchainRef swapchain = nullptr;
  vkb::Instance vkb_instance;
  vkb::Device vkb_device;
  double old_time = 0;
  uint32_t num_frames = 0;
  vuk::Unique<std::array<VkSemaphore, 3>> present_ready;
  vuk::Unique<std::array<VkSemaphore, 3>> render_complete;

  std::string device_name = {};
  uint32_t driver_version = {};

  VulkanContext() = default;

  static void init();

  void create_context(const AppSpec& spec);
  
  vuk::Allocator begin();
  void end(const vuk::Future& src, vuk::Allocator frame_allocator);

  static VulkanContext* get() { return s_instance; }

private:
  vuk::SingleSwapchainRenderBundle m_bundle = {};

  static VulkanContext* s_instance;
};
}
