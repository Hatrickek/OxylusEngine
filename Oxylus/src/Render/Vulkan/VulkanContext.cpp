#include "oxpch.h"
#define VK_USE_PLATFORM_WIN32_KHR
#include "VulkanContext.h"

#include "Utils/VulkanUtils.h"
#include "Render/Window.h"

#include "Utils/Log.h"

namespace Oxylus {
  VulkanContext::VkContext VulkanContext::Context;
  VulkanContext::VkQueue VulkanContext::VulkanQueue;

  void VulkanContext::CreateContext(const AppSpec& spec) {
    Context.Instance = ContextUtils::CreateInstance(spec.Name,
      "Oxylus Engine",
      {},
      ContextUtils::GetInstanceExtensions(),
      VK_API_VERSION_1_3);

    if (!Context.Instance)
      OX_CORE_ERROR("Failed to create Vulkan Instance");

    //Physical Device
    Context.PhysicalDevices = Context.Instance.enumeratePhysicalDevices();
    Context.PhysicalDevice = Context.PhysicalDevices[0];
    Context.DeviceProperties = Context.PhysicalDevice.getProperties();

    OX_CORE_TRACE("Vulkan renderer initialized using device: {}", Context.DeviceProperties.deviceName.data());

    //Surface
    {
      VkSurfaceKHR _surface;
      glfwCreateWindowSurface(Context.Instance, Window::GetGLFWWindow(), nullptr, &_surface);
      Context.Surface = vk::SurfaceKHR(_surface);
    }
    //Queue
    VulkanQueue.queueFamilyProperties = Context.PhysicalDevice.getQueueFamilyProperties();
    VulkanQueue.graphicsQueueFamilyIndex =
      ContextUtils::FindGraphicsQueueFamilyIndex(VulkanQueue.queueFamilyProperties);
    VulkanQueue.presentQueueFamilyIndex = Context.PhysicalDevice.getSurfaceSupportKHR(
                                            VulkanQueue.graphicsQueueFamilyIndex,
                                            Context.Surface)
                                            ? VulkanQueue.graphicsQueueFamilyIndex
                                            : (uint32_t)VulkanQueue.queueFamilyProperties.size();

    if (VulkanQueue.presentQueueFamilyIndex == VulkanQueue.queueFamilyProperties.size()) {
      // the graphicsQueueFamilyIndex doesn't support present -> look for an other family index that supports both
      // graphics and present
      for (uint32_t i = 0; i < VulkanQueue.queueFamilyProperties.size(); i++) {
        if ((VulkanQueue.queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics) && Context.PhysicalDevice.
            getSurfaceSupportKHR(i, Context.Surface)) {
          VulkanQueue.graphicsQueueFamilyIndex = i;
          VulkanQueue.presentQueueFamilyIndex = i;
          break;
        }
      }
      if (VulkanQueue.presentQueueFamilyIndex == VulkanQueue.queueFamilyProperties.size()) {
        // there's nothing like a single family index that supports both graphics and present -> look for an other
        // family index that supports present
        for (uint32_t i = 0; i < VulkanQueue.queueFamilyProperties.size(); i++) {
          if (Context.PhysicalDevice.getSurfaceSupportKHR(i, Context.Surface)) {
            VulkanQueue.presentQueueFamilyIndex = i;
            break;
          }
        }
      }
    }
    if ((VulkanQueue.graphicsQueueFamilyIndex == VulkanQueue.queueFamilyProperties.size()) || (
          VulkanQueue.presentQueueFamilyIndex == VulkanQueue.queueFamilyProperties.size())) {
      throw std::runtime_error("Could not find a queue for graphics or present -> terminating");
    }

    Context.DeviceMemoryProperties = Context.PhysicalDevice.getMemoryProperties();

    //Logical Device
    vk::PhysicalDeviceVulkan12Features features{};
    features.bufferDeviceAddress = VK_TRUE;
    features.bufferDeviceAddressCaptureReplay = VK_TRUE;

    vk::PhysicalDeviceVulkan13Features features13;
    features13.maintenance4 = VK_TRUE;
    features.pNext = &features13;
    Context.DeviceFeatures = Context.PhysicalDevice.getFeatures();
    Context.DeviceFeatures.depthClamp = VK_TRUE;
    Context.Device = ContextUtils::CreateDevice(Context.PhysicalDevice,
      VulkanQueue.graphicsQueueFamilyIndex,
      ContextUtils::GetDeviceExtensions(),
      &Context.DeviceFeatures,
      &features);

    //Get queues.
    VulkanQueue.GraphicsQueue = Context.Device.getQueue(VulkanQueue.graphicsQueueFamilyIndex, 0);
    //VulkanQueue.ComputeQueue = Context.Device.getQueue(VulkanQueue.graphicsQueueFamilyIndex, 1);
    VulkanQueue.PresentQueue = Context.Device.getQueue(VulkanQueue.presentQueueFamilyIndex, 0);
  }
}
