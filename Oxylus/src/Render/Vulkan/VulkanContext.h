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
  VkDevice Device = nullptr;
  VkPhysicalDevice PhysicalDevice = nullptr;
  VkQueue GraphicsQueue = nullptr;
  VkQueue TransferQueue = nullptr;
  std::optional<vuk::Context> Context;
  std::optional<vuk::DeviceSuperFrameResource> SuperframeResource;
  std::optional<vuk::Allocator> SuperframeAllocator;
  bool HasRt = false;
  bool Suspend = false;
  vuk::SwapchainRef Swapchain = nullptr;
  vkb::Instance VkbInstance;
  vkb::Device VkbDevice;
  double OldTime = 0;
  uint32_t NumFrames = 0;
  vuk::Unique<std::array<VkSemaphore, 3>> PresentReady;
  vuk::Unique<std::array<VkSemaphore, 3>> RenderComplete;

  std::string DeviceName = {};
  uint32_t DriverVersion = {};

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
