#include "Application.h"

#include <filesystem>

#include "Core.h"
#include "Layer.h"
#include "Render/Window.h"
#include "Render/Vulkan/VulkanContext.h"
#include "Render/Vulkan/VulkanRenderer.h"
#include "Systems/SystemManager.h"
#include "Utils/Profiler.h"

namespace Oxylus {
Application* Application::instance = nullptr;

Application::Application(AppSpec spec) : m_spec(std::move(spec)), system_manager(create_ref<SystemManager>()) {
  if (instance) {
    OX_CORE_ERROR("Application already exists!");
    return;
  }

  instance = this;

  if (m_spec.working_directory.empty())
    m_spec.working_directory = std::filesystem::current_path().string();
  else
    std::filesystem::current_path(m_spec.working_directory);

  for (int i = 0; i < m_spec.command_line_args.count; i++) {
    auto c = m_spec.command_line_args.args[i];
    if (!std::string(c).empty()) {
      command_line_args.emplace_back(c);
    }
  }

  if (!core.Init(m_spec))
    return;

  glfwSetKeyCallback(Window::get_glfw_window(),
    [](GLFWwindow* window, int key, int scancode, int action, int mods) {
      const auto app = get();

      if (action == GLFW_PRESS) {
        for (const auto& layer : app->layer_stack)
          layer->on_key_pressed((KeyCode)key);
      }
      else if (action == GLFW_RELEASE) {
        for (const auto& layer : app->layer_stack)
          layer->on_key_released((KeyCode)key);
      }
    });

  imgui_layer = new ImGuiLayer();
  push_overlay(imgui_layer);
}

Application::~Application() {
  close();
}

void Application::init_systems() {
  for (const auto& system : system_manager->GetSystems()) {
    system.second->SetDispatcher(&dispatcher);
    system.second->OnInit();
  }
}

Application& Application::push_layer(Layer* layer) {
  layer_stack.PushLayer(layer);
  layer->on_attach(dispatcher);

  return *this;
}

Application& Application::push_overlay(Layer* layer) {
  layer_stack.PushOverlay(layer);
  layer->on_attach(dispatcher);

  return *this;
}

void Application::run() {
  while (is_running) {
    Window::update_window();
    while (VulkanContext::get()->suspend) {
      Window::wait_for_events();
    }


    const auto time = static_cast<float>(glfwGetTime());
    timestep = time - last_frame_time;
    last_frame_time = time;

    // Layers
    update_layers(timestep);

    // Systems
    system_manager->OnUpdate();

    if (!is_running)
      break;

    update_renderer();

    FrameMark;
  }

  system_manager->Shutdown();
  core.Shutdown();
}

void Application::update_layers(Timestep ts) {
  OX_SCOPED_ZONE_N("LayersLoop");
  for (Layer* layer : layer_stack)
    layer->on_update(ts);
}

void Application::update_renderer() {
  OX_SCOPED_ZONE_N("SwapchainLoop");
  VulkanRenderer::draw(VulkanContext::get(), imgui_layer, layer_stack, system_manager);
}

void Application::close() {
  is_running = false;
}
}
