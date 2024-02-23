#include "VulkanContext.h"

#include <sstream>

#include "Render/Window.h"
#include "Utils/Log.h"
#include "Utils/Profiler.h"

#include <vuk/Context.hpp>
#include <vuk/Future.hpp>
#include <vuk/RenderGraph.hpp>
#include <vuk/resources/DeviceFrameResource.hpp>

#include "GLFW/glfw3.h"

namespace Ox {
VulkanContext* VulkanContext::s_instance = nullptr;

static VkBool32 DebugCallback(const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
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

  std::stringstream debug_message;
  debug_message << prefix << "[" << pCallbackData->messageIdNumber << "][" << pCallbackData->pMessageIdName << "] : " << pCallbackData->pMessage;

  if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
    OX_CORE_WARN("{}", debug_message.str());
  }
  else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    OX_CORE_INFO("{}", debug_message.str());
  }
  else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    fmt::println("{}", debug_message.str());
    OX_DEBUGBREAK();
  }
  else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    fmt::println("{}", debug_message.str());
    OX_DEBUGBREAK();
  }
  return VK_FALSE;
}

static vuk::Swapchain make_swapchain(VulkanContext* context, std::optional<VkSwapchainKHR> old_swapchain, vuk::PresentModeKHR present_mode = vuk::PresentModeKHR::eFifo) {
  context->context->wait_idle();
  vkb::SwapchainBuilder swb(context->vkb_device);
  swb.set_desired_format(vuk::SurfaceFormatKHR{vuk::Format::eR8G8B8A8Unorm, vuk::ColorSpaceKHR::eSrgbNonlinear});
  swb.set_desired_present_mode((VkPresentModeKHR)present_mode);
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
  sw.surface = context->vkb_device.surface;
  sw.swapchain = vkswapchain->swapchain;
  return sw;
}

void VulkanContext::init() {
  if (s_instance)
    return;

  s_instance = new VulkanContext();
}

inline VkSurfaceKHR create_surface_glfw(const VkInstance instance, GLFWwindow* window) {
  VkSurfaceKHR surface = nullptr;
  const VkResult err = glfwCreateWindowSurface(instance, window, nullptr, &surface);
  if (err) {
    const char* error_msg;
    const int ret = glfwGetError(&error_msg);
    if (ret != 0) {
      OX_CORE_ERROR("GLFW error: {}", error_msg);
    }
    surface = nullptr;
  }
  return surface;
}

void VulkanContext::create_context(const AppSpec& spec) {
  OX_SCOPED_ZONE;
  vkb::InstanceBuilder builder;
  builder.set_app_name("Oxylus")
         .set_engine_name("Oxylus")
         .require_api_version(1, 2, 0)
         .set_app_version(0, 1, 0);

  bool enable_validation = false;
  const auto& args = App::get()->get_command_line_args();
  for (const auto& arg : args) {
    if (arg == "vulkan-validation") {
      enable_validation = true;
      break;
    }
  }

  if (enable_validation) {
    OX_CORE_INFO("Vulkan validation layers enabled.");
    builder.request_validation_layers()
           .set_debug_callback([](const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                  const VkDebugUtilsMessageTypeFlagsEXT messageType,
                                  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                  void* pUserData) -> VkBool32 {
              return DebugCallback(messageSeverity, messageType, pCallbackData, pUserData);
            });
  }

  auto inst_ret = builder.build();
  if (!inst_ret) {
    OX_CORE_ERROR("Couldn't initialise instance");
  }

  vkb_instance = inst_ret.value();
  auto instance = vkb_instance.instance;
  vkb::PhysicalDeviceSelector selector{vkb_instance};
  surface = create_surface_glfw(instance, Window::get_glfw_window());
  selector.set_surface(surface)
          .set_minimum_version(1, 2)
          .add_required_extension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME)
          .add_required_extension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME)
          .add_required_extension(VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME);

  VkPhysicalDeviceFeatures2 vk10features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR};
  vk10features.features.geometryShader = true;
  vk10features.features.shaderInt64 = true;
  vk10features.features.shaderStorageImageWriteWithoutFormat = true;
  vk10features.features.depthClamp = true;
  vk10features.features.shaderStorageImageReadWithoutFormat = true;
  vk10features.features.fillModeNonSolid = true;
  vk10features.features.multiViewport = true;
  selector.set_required_features(vk10features.features);
  VkPhysicalDeviceVulkan11Features vk11features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
  vk11features.shaderDrawParameters = true;
  selector.set_required_features_11(vk11features);
  VkPhysicalDeviceVulkan12Features vk12features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
  vk12features.timelineSemaphore = true;
  vk12features.descriptorBindingPartiallyBound = true;
  vk12features.descriptorBindingUpdateUnusedWhilePending = true;
  vk12features.runtimeDescriptorArray = true;
  vk12features.descriptorBindingVariableDescriptorCount = true;
  vk12features.hostQueryReset = true;
  vk12features.bufferDeviceAddress = true;
  vk12features.shaderOutputLayer = true;
  vk12features.descriptorIndexing = true;
  vk12features.shaderUniformBufferArrayNonUniformIndexing = true;
  vk12features.shaderSampledImageArrayNonUniformIndexing = true;
  vk12features.shaderStorageBufferArrayNonUniformIndexing = true;
  vk12features.shaderStorageImageArrayNonUniformIndexing = true;
  vk12features.shaderInputAttachmentArrayNonUniformIndexing = true;
  vk12features.shaderUniformTexelBufferArrayNonUniformIndexing = true;
  vk12features.shaderStorageTexelBufferArrayNonUniformIndexing = true;
  vk12features.shaderOutputViewportIndex = true;
  selector.set_required_features_12(vk12features);

  VkPhysicalDeviceSynchronization2FeaturesKHR sync_feat{
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR,
    .synchronization2 = true
  };

  selector.add_required_extension_features<>(sync_feat);

  auto phys_ret = selector.select();
  vkb::PhysicalDevice vkbphysical_device;
  if (!phys_ret) {
    has_rt = false;
    vkb::PhysicalDeviceSelector selector2{vkb_instance};
    selector2.set_surface(surface).set_minimum_version(1, 2).add_required_extension(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
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

  auto dev_ret = device_builder.build();
  if (!dev_ret) {
    OX_CORE_ERROR("Couldn't create device");
  }
  vkb_device = dev_ret.value();
  graphics_queue = vkb_device.get_queue(vkb::QueueType::graphics).value();
  graphics_queue_family_index = vkb_device.get_queue_index(vkb::QueueType::graphics).value();
  transfer_queue = vkb_device.get_queue(vkb::QueueType::transfer).value();
  auto transfer_queue_family_index = vkb_device.get_queue_index(vkb::QueueType::transfer).value();
  device = vkb_device.device;
  vuk::ContextCreateParameters::FunctionPointers fps;
  fps.vkGetInstanceProcAddr = vkb_instance.fp_vkGetInstanceProcAddr;
  fps.vkGetDeviceProcAddr = vkb_instance.fp_vkGetDeviceProcAddr;
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
  superframe_resource.emplace(*context, num_inflight_frames);
  superframe_allocator.emplace(*superframe_resource);
  auto sw = make_swapchain(this, {}, present_mode);
  swapchain = context->add_swapchain(sw);
  present_ready = vuk::Unique<std::array<VkSemaphore, 3>>(*superframe_allocator);
  render_complete = vuk::Unique<std::array<VkSemaphore, 3>>(*superframe_allocator);

  context->set_shader_target_version(VK_API_VERSION_1_2);

  superframe_allocator->allocate_semaphores(*present_ready);
  superframe_allocator->allocate_semaphores(*render_complete);

  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(physical_device, &properties);

  device_name = properties.deviceName;
  driver_version = properties.driverVersion;

#ifdef TRACY_ENABLE
  tracy_profiler = create_ref<TracyProfiler>();
  tracy_profiler->init_tracy_for_vulkan(this);
#endif

  Window::set_window_user_data(this);
  glfwSetWindowSizeCallback(Window::get_glfw_window(),
                            [](GLFWwindow* window, const int width, const int height) {
                              auto* ctx = reinterpret_cast<VulkanContext*>(glfwGetWindowUserPointer(window));
                              if (width == 0 && height == 0) {
                                ctx->suspend = true;
                              }
                              else {
                                ctx->rebuild_swapchain();
                              }
                            });

  OX_CORE_INFO("Vulkan context initialized using device: {}", device_name);
}

void VulkanContext::rebuild_swapchain(const vuk::PresentModeKHR new_present_mode) {
  context->wait_idle();
  superframe_allocator->deallocate(std::span{&swapchain->swapchain, 1});
  superframe_allocator->deallocate(swapchain->image_views);
  context->remove_swapchain(swapchain);
  auto sw = make_swapchain(this, swapchain->swapchain, new_present_mode);
  swapchain = context->add_swapchain(sw);
  for (auto& iv : swapchain->image_views) {
    context->set_name(iv.payload, "Swapchain ImageView");
  }
  suspend = false;
  present_mode = new_present_mode;
}

vuk::Allocator VulkanContext::begin() {
  auto& frame_resource = superframe_resource->get_next_frame();
  context->next_frame();
  const vuk::Allocator frame_allocator(frame_resource);
  m_bundle = *acquire_one(*context, swapchain, (*present_ready)[context->get_frame_count() % 3], (*render_complete)[context->get_frame_count() % 3]);

  return frame_allocator;
}

void VulkanContext::end(const vuk::Future& src, vuk::Allocator frame_allocator) {
  std::shared_ptr rg_p(std::make_shared<vuk::RenderGraph>("presenter"));
  rg_p->attach_in("_src", src);
  rg_p->release_for_present("_src");

  vuk::ProfilingCallbacks cbs;
#if GPU_PROFILER_ENABLED
  cbs.on_begin_command_buffer = [](void*, const VkCommandBuffer cbuf) {
    get()->tracy_profiler->get_graphics_ctx()->Collect(cbuf);
    get()->tracy_profiler->get_transfer_ctx()->Collect(cbuf);
    return (void*)nullptr;
  };

  cbs.on_begin_pass = [](void*, const vuk::Name pass_name, const VkCommandBuffer cmdbuf, const vuk::DomainFlagBits domain) {
    void* pass_data = new char[sizeof(tracy::VkCtxScope)];
    if (domain & vuk::DomainFlagBits::eGraphicsQueue) {
      new(pass_data) OX_TRACE_GPU_TRANSIENT(get()->tracy_profiler->get_graphics_ctx(), cmdbuf, pass_name.c_str())
    }
    else if (domain & vuk::DomainFlagBits::eTransferQueue) {
      new(pass_data) OX_TRACE_GPU_TRANSIENT(get()->tracy_profiler->get_transfer_ctx(), cmdbuf, pass_name.c_str())
    }

    return pass_data;
  };

  cbs.on_end_pass = [](void*, void* pass_data) {
    const auto tracy_scope = reinterpret_cast<tracy::VkCtxScope*>(pass_data);
    tracy_scope->~VkCtxScope();
  };
#endif

  vuk::Compiler compiler;
  auto erg = *compiler.link(std::span{&rg_p, 1}, {.callbacks = cbs});
  auto result = *execute_submit(frame_allocator, std::move(erg), std::move(m_bundle));
  present_to_one(*context, std::move(result));

  current_frame = (current_frame + 1) % num_inflight_frames;
  num_frames = context->get_frame_count();
}
}
