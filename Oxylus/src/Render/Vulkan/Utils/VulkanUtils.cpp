#include "src/oxpch.h"
#include "VulkanUtils.h"

#include <GLFW/glfw3.h>

#define DISABLE_DEBUG_LAYERS 0

namespace Oxylus {
  PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;
  PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;
  PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameExt;
  VkDebugUtilsMessengerEXT debugUtilsMessenger;

  static std::vector<char const*> GatherLayers(std::vector<std::string> const& layers,
                                               std::vector<vk::LayerProperties> const& layerProperties) {
    std::vector<char const*> enabledLayers;
    enabledLayers.reserve(layers.size());
    for (auto const& layer : layers) {
      assert(std::find_if(layerProperties.begin(), layerProperties.end(),
        [layer](vk::LayerProperties const& lp) { return layer == lp.layerName; }) != layerProperties.end());
      enabledLayers.push_back(layer.data());
    }
#if !(DISABLE_DEBUG_LAYERS)
    // Enable standard validation layer to find as much errors as possible!
    enabledLayers.push_back("VK_LAYER_KHRONOS_validation");
#endif
    return enabledLayers;
  }

  static std::vector<char const*> GatherExtensions(std::vector<std::string> const& extensions,
                                                   std::vector<vk::ExtensionProperties> const& extensionProperties) {
    std::vector<char const*> enabledExtensions;
    enabledExtensions.reserve(extensions.size());
    for (auto const& ext : extensions) {
      assert(std::find_if(extensionProperties.begin(),
          extensionProperties.end(), [ext](vk::ExtensionProperties const& ep) { return ext == ep.extensionName; }) !=
        extensionProperties.end());
      enabledExtensions.push_back(ext.data());
    }
#if ( !DISABLE_DEBUG_LAYERS )
    if (std::find(extensions.begin(), extensions.end(), VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == extensions.end() &&
        std::find_if(extensionProperties.begin(),
          extensionProperties.end(),
          [](vk::ExtensionProperties const& ep) {
            return (strcmp(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, ep.extensionName) == 0);
          }) != extensionProperties.end()) {
      enabledExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
#endif
    return enabledExtensions;
  }

  VKAPI_ATTR VkBool32 VKAPI_CALL debugUtilsMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                             VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                                             VkDebugUtilsMessengerCallbackDataEXT const* pCallbackData,
                                                             void* pUserData) {
    // Select prefix depending on flags passed to the callback
    std::string prefix;

    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
      prefix = "VULKAN VERBOSE: ";
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
      prefix = "VULKAN INFO: ";
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
      prefix = "VULKAN WARNING: ";
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
      prefix = "VULKAN ERROR: ";
    }

    std::stringstream debugMessage;
    debugMessage << prefix << "[" << pCallbackData->messageIdNumber << "][" << pCallbackData->pMessageIdName << "] : "
      << pCallbackData->pMessage;

    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
      OX_CORE_WARN(debugMessage.str());
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
      OX_CORE_INFO(debugMessage.str());
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
      OX_CORE_WARN(debugMessage.str());
      OX_DEBUGBREAK();
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
      OX_CORE_BERROR(debugMessage.str());
    }

    return VK_FALSE;
  }

#if ( DISABLE_DEBUG_LAYERS )
	static vk::StructureChain<vk::InstanceCreateInfo>
#else

  static vk::StructureChain<vk::InstanceCreateInfo, vk::DebugUtilsMessengerCreateInfoEXT>
#endif
  MakeInstanceCreateInfoChain(vk::ApplicationInfo const& applicationInfo,
                              std::vector<char const*> const& layers,
                              std::vector<char const*> const& extensions) {
#if ( DISABLE_DEBUG_LAYERS )
		// in non-debug mode just use the InstanceCreateInfo for Instance creation
		vk::StructureChain<vk::InstanceCreateInfo> instanceCreateInfo(
            { vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR, &applicationInfo, layers, extensions });
#else
    // in debug mode, addionally use the debugUtilsMessengerCallback in Instance creation!
    vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
    vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
      vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
      vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
    vk::StructureChain<vk::InstanceCreateInfo, vk::DebugUtilsMessengerCreateInfoEXT> instanceCreateInfo(
      {vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR, &applicationInfo, layers, extensions},
      {{}, severityFlags, messageTypeFlags, &debugUtilsMessengerCallback});
#endif
    return instanceCreateInfo;
  }

  VkBool32 DebugReportCallback(VkDebugReportFlagsEXT flags,
                               VkDebugReportObjectTypeEXT objectType,
                               uint64_t object,
                               size_t location,
                               int32_t messageCode,
                               const char* pLayerPrefix,
                               const char* pMessage,
                               void* pUserData) {
    OX_CORE_WARN("VulkanDebugCallback:\n Object Type: {0}\n Message: {1}", objectType, messageCode);
    OX_DEBUGBREAK();
    return VK_FALSE;
  }

  void VulkanUtils::SetObjectName(const uint64_t objectHandle, const vk::ObjectType objectType, const char* objectName) {
#ifndef OX_DIST
    vk::DebugUtilsObjectNameInfoEXT objectNameInfo = {};
    objectNameInfo.objectHandle = objectHandle;
    objectNameInfo.objectType = objectType;
    objectNameInfo.pObjectName = objectName;
    const VkDebugUtilsObjectNameInfoEXT casted = objectNameInfo;
    CheckResult(vkSetDebugUtilsObjectNameExt(VulkanContext::GetDevice(), &casted));
#endif
  }

  vk::Instance ContextUtils::CreateInstance(std::string const& appName,
                                            std::string const& engineName,
                                            std::vector<std::string> const& layers,
                                            std::vector<std::string> const& extensions,
                                            uint32_t apiVersion) {
    const vk::ApplicationInfo applicationInfo(appName.c_str(), 1, engineName.c_str(), 1, apiVersion);
    const std::vector<char const*> enabledLayers = GatherLayers(layers, vk::enumerateInstanceLayerProperties().value);
    std::vector<char const*> enabledExtensions = GatherExtensions(extensions,
      vk::enumerateInstanceExtensionProperties().value);
    uint32_t count = 0;
    const char** names = glfwGetRequiredInstanceExtensions(&count);
    if (count && names) {
      for (uint32_t i = 0; i < count; i++) {
        enabledExtensions.emplace_back(names[i]);
      }
    }
    enabledExtensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    const vk::Instance instance = vk::createInstance(
      MakeInstanceCreateInfoChain(applicationInfo, enabledLayers, enabledExtensions).get<vk::InstanceCreateInfo>()).value;
#if (!DISABLE_DEBUG_LAYERS)
    vkCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(
      instance,
      "vkCreateDebugUtilsMessengerEXT"));
    vkDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(
      instance,
      "vkDestroyDebugUtilsMessengerEXT"));
    vkSetDebugUtilsObjectNameExt = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT"));

    VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCI{};
    debugUtilsMessengerCI.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugUtilsMessengerCI.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugUtilsMessengerCI.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    debugUtilsMessengerCI.pfnUserCallback = debugUtilsMessengerCallback;
    VkResult result = vkCreateDebugUtilsMessengerEXT(instance, &debugUtilsMessengerCI, nullptr, &debugUtilsMessenger);
    assert(result == VK_SUCCESS);
#endif
    return instance;
  }

  std::vector<std::string> ContextUtils::GetInstanceExtensions() {
    std::vector<std::string> extensions;
    extensions.emplace_back(VK_KHR_SURFACE_EXTENSION_NAME);
#if defined( VK_USE_PLATFORM_ANDROID_KHR )
		extensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif defined( VK_USE_PLATFORM_IOS_MVK )
		extensions.push_back(VK_MVK_IOS_SURFACE_EXTENSION_NAME);
#elif defined( VK_USE_PLATFORM_MACOS_MVK )
		extensions.push_back(VK_MVK_MACOS_SURFACE_EXTENSION_NAME);
#elif defined( VK_USE_PLATFORM_MIR_KHR )
		extensions.push_back(VK_KHR_MIR_SURFACE_EXTENSION_NAME);
#elif defined( VK_USE_PLATFORM_VI_NN )
		extensions.push_back(VK_NN_VI_SURFACE_EXTENSION_NAME);
#elif defined( VK_USE_PLATFORM_WAYLAND_KHR )
		extensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#elif defined( VK_USE_PLATFORM_WIN32_KHR )
    extensions.emplace_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined( VK_USE_PLATFORM_XCB_KHR )
		extensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#elif defined( VK_USE_PLATFORM_XLIB_KHR )
		extensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#elif defined( VK_USE_PLATFORM_XLIB_XRANDR_EXT )
		extensions.push_back(VK_EXT_ACQUIRE_XLIB_DISPLAY_EXTENSION_NAME);
#endif
    return extensions;
  }

  uint32_t ContextUtils::FindGraphicsQueueFamilyIndex(
    std::vector<vk::QueueFamilyProperties> const& queueFamilyProperties) {
    // get the first index into queueFamiliyProperties which supports graphics
    const auto graphicsQueueFamilyProperty = std::find_if(queueFamilyProperties.begin(),
      queueFamilyProperties.end(),
      [](vk::QueueFamilyProperties const& qfp) {
        return qfp.queueFlags & vk::QueueFlagBits::eGraphics;
      });
    assert(graphicsQueueFamilyProperty != queueFamilyProperties.end());
    return static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperty));
  }

  vk::Device ContextUtils::CreateDevice(const vk::PhysicalDevice& physicalDevice,
                                        uint32_t queueFamilyIndex,
                                        const std::vector<std::string>& extensions,
                                        const vk::PhysicalDeviceFeatures* physicalDeviceFeatures,
                                        const void* pNext) {
    std::vector<char const*> enabledExtensions;
    enabledExtensions.reserve(extensions.size());
    for (auto const& ext : extensions) {
      enabledExtensions.push_back(ext.data());
    }

    //    enabledExtensions.emplace_back("VK_EXT_calibrated_timestamps");
#if 0
    enabledExtensions.emplace_back("VK_KHR_portability_subset");
#endif

    constexpr float queuePriority = 0.0f;
    vk::DeviceQueueCreateInfo deviceQueueCreateInfo({}, queueFamilyIndex, 1, &queuePriority);
    const vk::DeviceCreateInfo deviceCreateInfo({},
      deviceQueueCreateInfo,
      {},
      enabledExtensions,
      physicalDeviceFeatures,
      pNext);

    const vk::Device device = physicalDevice.createDevice(deviceCreateInfo).value;

    return device;
  }
}
