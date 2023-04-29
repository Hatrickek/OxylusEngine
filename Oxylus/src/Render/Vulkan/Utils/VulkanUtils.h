#pragma once

#include <vulkan/vulkan.hpp>

#include "Render/Vulkan/VulkanContext.h"
#include "Utils/Log.h"

namespace Oxylus {
  class VulkanUtils {
  public:
    static void CheckResult(VkResult result) {
      if (result != VK_SUCCESS) {
        OX_CORE_BERROR("Vulkan Error: {}", (int)result);
      }
    }

    static bool CheckResult(vk::Result result) {
      //vk::resultCheck(result, "Vulkan Exception: \n");
      if (result != vk::Result::eSuccess) {
        OX_DEBUGBREAK();
        return false;
      }
      return true;
    }

    static bool IsOutOfDate(vk::Result result) {
      if (result == vk::Result::eErrorOutOfDateKHR) {
        return true;
      }
      return false;
    }

    static bool IsSubOptimal(vk::Result result) {
      if (result == vk::Result::eSuboptimalKHR) {
        return true;
      }
      return false;
    }

    static vk::Bool32 getMemoryType(uint32_t typeBits, const vk::MemoryPropertyFlags& properties, uint32_t* typeIndex) {
      for (uint32_t i = 0; i < 32; i++) {
        if ((typeBits & 1) == 1) {
          if ((VulkanContext::Context.DeviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            *typeIndex = i;
            return VK_TRUE;
          }
        }
        typeBits >>= 1;
      }
      return VK_FALSE;
    }

    static uint32_t getMemoryType(uint32_t typeBits, const vk::MemoryPropertyFlags& properties) {
      uint32_t result = 0;
      if (VK_FALSE == getMemoryType(typeBits, properties, &result)) {
        OX_CORE_BERROR("Unable to find memory type");
      }
      return result;
    }

    static void SetObjectName(uint64_t objectHandle, vk::ObjectType objectType, const char* objectName);
  };

  class ContextUtils {
  public:
    static vk::Instance CreateInstance(std::string const& appName,
                                       std::string const& engineName,
                                       std::vector<std::string> const& layers,
                                       std::vector<std::string> const& extensions,
                                       uint32_t apiVersion = VK_API_VERSION_1_0);

    static std::vector<std::string> GetInstanceExtensions();

    static vk::Device CreateDevice(vk::PhysicalDevice const& physicalDevice,
                                   uint32_t queueFamilyIndex,
                                   std::vector<std::string> const& extensions,
                                   vk::PhysicalDeviceFeatures const* physicalDeviceFeatures,
                                   void const* pNext = nullptr);

    static std::vector<std::string> GetDeviceExtensions() {
      return {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    }

    static uint32_t FindGraphicsQueueFamilyIndex(std::vector<vk::QueueFamilyProperties> const& queueFamilyProperties);
  };
}
