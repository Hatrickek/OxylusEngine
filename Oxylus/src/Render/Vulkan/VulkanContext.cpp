#include "VulkanContext.h"

#include "Render/Window.h"
#include "Utils/Log.h"

#include <vuk/Context.hpp>
#include <vuk/Future.hpp>
#include <vuk/RenderGraph.hpp>
#include <vuk/resources/DeviceFrameResource.hpp>

namespace Oxylus {
VulkanContext* VulkanContext::s_Instance = nullptr;

static VkBool32 DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                              VkDebugUtilsMessageTypeFlagsEXT messageType,
                              const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                              void* pUserData) {
  std::string prefix;
  if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
    prefix = "VULKAN VERBOSE: ";
  }
  else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    prefix = "VULKAN INFO: ";
  }
  else if (messageSeverity &
           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    prefix = "VULKAN WARNING: ";
  }
  else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    prefix = "VULKAN ERROR: ";
  }

  std::stringstream debugMessage;
  debugMessage << prefix << "[" << pCallbackData->messageIdNumber << "][" << pCallbackData->pMessageIdName << "] : " << pCallbackData->pMessage;

  if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
    OX_CORE_WARN(debugMessage.str());
  }
  else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    OX_CORE_INFO(debugMessage.str());
  }
  else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    OX_CORE_WARN(debugMessage.str());
    OX_DEBUGBREAK()
  }
  else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    OX_CORE_ERROR(debugMessage.str());
    OX_DEBUGBREAK()
  }
  return VK_FALSE;
}

inline vuk::Swapchain MakeSwapchain(vkb::Device vkbdevice, std::optional<VkSwapchainKHR> old_swapchain) {
  vkb::SwapchainBuilder swb(vkbdevice);
  swb.set_desired_format(vuk::SurfaceFormatKHR{vuk::Format::eR8G8B8A8Unorm, vuk::ColorSpaceKHR::eSrgbNonlinear});
  swb.add_fallback_format(vuk::SurfaceFormatKHR{vuk::Format::eR8G8B8A8Unorm, vuk::ColorSpaceKHR::eSrgbNonlinear});
  swb.set_desired_present_mode((VkPresentModeKHR)vuk::PresentModeKHR::eImmediate);
  swb.set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
  if (old_swapchain) {
    swb.set_old_swapchain(*old_swapchain);
  }
  auto vkswapchain = swb.build();

  vuk::Swapchain sw{};
  auto images = *vkswapchain->get_images();
  auto views = *vkswapchain->get_image_views();

  for (auto& i : images) {
    sw.images.push_back(vuk::Image{i, nullptr});
  }
  for (auto& i : views) {
    sw.image_views.emplace_back();
    sw.image_views.back().payload = i;
  }
  sw.extent = vuk::Extent2D{vkswapchain->extent.width, vkswapchain->extent.height};
  sw.format = vuk::Format(vkswapchain->image_format);
  sw.surface = vkbdevice.surface;
  sw.swapchain = vkswapchain->swapchain;
  return sw;
}

void VulkanContext::Init() {
  if (s_Instance)
    return;

  s_Instance = new VulkanContext();
}

void VulkanContext::CreateContext(const AppSpec& spec) {
  vkb::InstanceBuilder builder;
  builder.set_app_name("Oxylus")
         .set_engine_name("Oxylus")
         .require_api_version(1, 2, 0)
         .set_app_version(0, 1, 0);

  bool enableValidation = false;
  const auto& args = Application::Get()->GetCommandLineArgs();
  for (const auto& arg : args) {
    if (arg == "vulkan-validation") {
      enableValidation = true;
      break;
    }
  }

  if (enableValidation) {
    OX_CORE_TRACE("Vulkan validation layers enabled.");
    builder.request_validation_layers()
           .set_debug_callback([](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                  VkDebugUtilsMessageTypeFlagsEXT messageType,
                                  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                  void* pUserData) -> VkBool32 {
              return DebugCallback(messageSeverity, messageType, pCallbackData, pUserData);
            });
  }

  auto inst_ret = builder.build();
  if (!inst_ret) {
    OX_CORE_ERROR("Couldn't initialise instance");
  }

  vkbinstance = inst_ret.value();
  auto instance = vkbinstance.instance;
  vkb::PhysicalDeviceSelector selector{vkbinstance};
  VkSurfaceKHR surface;
  glfwSetWindowUserPointer(Window::GetGLFWWindow(), this);
  glfwSetWindowSizeCallback(Window::GetGLFWWindow(),
    [](GLFWwindow* window, int width, int height) {
      VulkanContext& runner = *reinterpret_cast<VulkanContext*>(glfwGetWindowUserPointer(window));
      if (width == 0 && height == 0) {
        runner.suspend = true;
      }
      else {
        Window::GetWindowExtent().height = height;
        Window::GetWindowExtent().width = width;
        runner.superframe_allocator->deallocate(std::span{&runner.swapchain->swapchain, 1});
        runner.superframe_allocator->deallocate(runner.swapchain->image_views);
        runner.context->remove_swapchain(runner.swapchain);
        runner.swapchain = runner.context->add_swapchain(MakeSwapchain(runner.vkbdevice, runner.swapchain->swapchain));
        for (auto& iv : runner.swapchain->image_views) {
          runner.context->set_name(iv.payload, "Swapchain ImageView");
        }
        runner.suspend = false;
      }
    });
  glfwCreateWindowSurface(instance, Window::GetGLFWWindow(), nullptr, &surface);
  selector.set_surface(surface)
          .set_minimum_version(1, 0)
          .add_required_extension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME)
          .add_required_extension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME)
          .add_required_extension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME)
          .add_required_extension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
  auto phys_ret = selector.select();
  vkb::PhysicalDevice vkbphysical_device;
  if (!phys_ret) {
    hasRT = false;
    vkb::PhysicalDeviceSelector selector2{vkbinstance};
    selector2.set_surface(surface).set_minimum_version(1, 0).add_required_extension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
    auto phys_ret2 = selector2.select();
    if (!phys_ret2) {
      OX_CORE_ERROR("Couldn't create physical device");
    }
    else {
      vkbphysical_device = phys_ret2.value();
    }
  }
  else {
    vkbphysical_device = phys_ret.value();
  }

  physical_device = vkbphysical_device.physical_device;
  vkb::DeviceBuilder device_builder{vkbphysical_device};
  VkPhysicalDeviceVulkan12Features vk12features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
  vk12features.timelineSemaphore = true;
  vk12features.descriptorBindingPartiallyBound = true;
  vk12features.descriptorBindingUpdateUnusedWhilePending = true;
  vk12features.shaderSampledImageArrayNonUniformIndexing = true;
  vk12features.runtimeDescriptorArray = true;
  vk12features.descriptorBindingVariableDescriptorCount = true;
  vk12features.hostQueryReset = true;
  vk12features.bufferDeviceAddress = true;
  vk12features.shaderOutputLayer = true;
  vk12features.descriptorIndexing = true;
  vk12features.shaderInputAttachmentArrayNonUniformIndexing = true;
  vk12features.shaderUniformBufferArrayNonUniformIndexing = true;
  VkPhysicalDeviceVulkan11Features vk11features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
  vk11features.shaderDrawParameters = true;
  VkPhysicalDeviceFeatures2 vk10features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR};
  vk10features.features.shaderInt64 = true;
  vk10features.features.shaderStorageImageWriteWithoutFormat = true;
  VkPhysicalDeviceSynchronization2FeaturesKHR sync_feat{
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR,
    .synchronization2 = true
  };
  VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeature{
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
    .accelerationStructure = true
  };
  VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeature{
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
    .rayTracingPipeline = true
  };
  device_builder = device_builder.add_pNext(&vk12features).add_pNext(&vk11features).add_pNext(&sync_feat).add_pNext(&accelFeature).add_pNext(&vk10features);
  if (hasRT) {
    device_builder = device_builder.add_pNext(&rtPipelineFeature);
  }
  auto dev_ret = device_builder.build();
  if (!dev_ret) {
    OX_CORE_ERROR("Couldn't create device");
  }
  vkbdevice = dev_ret.value();
  graphics_queue = vkbdevice.get_queue(vkb::QueueType::graphics).value();
  auto graphics_queue_family_index = vkbdevice.get_queue_index(vkb::QueueType::graphics).value();
  transfer_queue = vkbdevice.get_queue(vkb::QueueType::transfer).value();
  auto transfer_queue_family_index = vkbdevice.get_queue_index(vkb::QueueType::transfer).value();
  device = vkbdevice.device;
  vuk::ContextCreateParameters::FunctionPointers fps;
  fps.vkGetInstanceProcAddr = vkbinstance.fp_vkGetInstanceProcAddr;
  fps.vkGetDeviceProcAddr = vkbinstance.fp_vkGetDeviceProcAddr;
  context.emplace(vuk::ContextCreateParameters{
    instance,
    device,
    physical_device,
    graphics_queue,
    graphics_queue_family_index,
    VK_NULL_HANDLE,
    VK_QUEUE_FAMILY_IGNORED,
    transfer_queue,
    transfer_queue_family_index,
    fps
  });
  constexpr unsigned num_inflight_frames = 3;
  superframe_resource.emplace(*context, num_inflight_frames);
  superframe_allocator.emplace(*superframe_resource);
  swapchain = context->add_swapchain(MakeSwapchain(vkbdevice, {}));
  present_ready = vuk::Unique<std::array<VkSemaphore, 3>>(*superframe_allocator);
  render_complete = vuk::Unique<std::array<VkSemaphore, 3>>(*superframe_allocator);

  superframe_allocator->allocate_semaphores(*present_ready);
  superframe_allocator->allocate_semaphores(*render_complete);

  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(physical_device, &properties);

  OX_CORE_TRACE("Vulkan context initialized using device: {}", properties.deviceName);
}

vuk::Allocator VulkanContext::Begin() {
  auto& frame_resource = superframe_resource->get_next_frame();
  context->next_frame();
  const vuk::Allocator frameAllocator(frame_resource);

  return frameAllocator;
}

void VulkanContext::End(const vuk::Future& src, vuk::Allocator frameAllocator) {
  std::shared_ptr rg_p(std::make_shared<vuk::RenderGraph>("presenter"));
  rg_p->attach_in("_src", src);
  rg_p->release_for_present("_src");
  vuk::Compiler compiler;
  auto erg = *compiler.link(std::span{&rg_p, 1}, {});
  m_Bundle = *acquire_one(*context, swapchain, (*present_ready)[context->get_frame_count() % 3], (*render_complete)[context->get_frame_count() % 3]);
  auto result = *execute_submit(frameAllocator, std::move(erg), std::move(m_Bundle));
  vuk::present_to_one(*context, std::move(result));
}
}
