#include "src/oxpch.h"
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
      OX_CORE_FATAL("Failed to create Vulkan Instance");

    //Physical Device
    Context.PhysicalDevices = Context.Instance.enumeratePhysicalDevices().value;
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
                                            Context.Surface).value
                                            ? VulkanQueue.graphicsQueueFamilyIndex
                                            : (uint32_t)VulkanQueue.queueFamilyProperties.size();

    if (VulkanQueue.presentQueueFamilyIndex == VulkanQueue.queueFamilyProperties.size()) {
      // the graphicsQueueFamilyIndex doesn't support present -> look for an other family index that supports both
      // graphics and present
      for (uint32_t i = 0; i < VulkanQueue.queueFamilyProperties.size(); i++) {
        if ((VulkanQueue.queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics) && Context.PhysicalDevice.
                                                                                                        getSurfaceSupportKHR(i, Context.Surface).value) {
          VulkanQueue.graphicsQueueFamilyIndex = i;
          VulkanQueue.presentQueueFamilyIndex = i;
          break;
        }
      }
      if (VulkanQueue.presentQueueFamilyIndex == VulkanQueue.queueFamilyProperties.size()) {
        // there's nothing like a single family index that supports both graphics and present -> look for an other
        // family index that supports present
        for (uint32_t i = 0; i < VulkanQueue.queueFamilyProperties.size(); i++) {
          if (Context.PhysicalDevice.getSurfaceSupportKHR(i, Context.Surface).value) {
            VulkanQueue.presentQueueFamilyIndex = i;
            break;
          }
        }
      }
    }
    if (VulkanQueue.graphicsQueueFamilyIndex == VulkanQueue.queueFamilyProperties.size() || (
          VulkanQueue.presentQueueFamilyIndex == VulkanQueue.queueFamilyProperties.size())) {
      OX_CORE_FATAL("Could not find a queue for graphics or present!");
    }

    Context.DeviceMemoryProperties = Context.PhysicalDevice.getMemoryProperties();

    //Logical Device
    vk::PhysicalDeviceVulkan13Features features{};
    features.maintenance4 = VK_TRUE;
    Context.DeviceFeatures = Context.PhysicalDevice.getFeatures();
    //TODO: Check if device supports these
    Context.DeviceFeatures.shaderUniformBufferArrayDynamicIndexing = true;
    Context.DeviceFeatures.shaderSampledImageArrayDynamicIndexing = true;
    Context.DeviceFeatures.shaderStorageImageArrayDynamicIndexing = true;
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

    // VMA
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.device = GetDevice();
    allocatorInfo.instance = GetInstance();
    allocatorInfo.physicalDevice = GetPhysicalDevice();
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    vmaCreateAllocator(&allocatorInfo, &Context.Allocator);
  }
}
