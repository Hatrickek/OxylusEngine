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
  bool hasRT = false;
  bool suspend = false;
  vuk::SwapchainRef swapchain = nullptr;
  vkb::Instance vkbinstance;
  vkb::Device vkbdevice;
  double old_time = 0;
  uint32_t num_frames = 0;
  vuk::Unique<std::array<VkSemaphore, 3>> present_ready;
  vuk::Unique<std::array<VkSemaphore, 3>> render_complete;

  VulkanContext() = default;

  static void Init();

  void CreateContext(const AppSpec& spec);
  
  vuk::Allocator Begin();
  void End(const vuk::Future& src, vuk::Allocator frameAllocator);

  static VulkanContext* Get() { return s_Instance; }

private:
  vuk::SingleSwapchainRenderBundle m_Bundle = {};

  static VulkanContext* s_Instance;
};
}
