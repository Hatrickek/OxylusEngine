#include "Application.h"

#include <filesystem>

#include "Core.h"
#include "Layer.h"
#include "Render/Window.h"
#include "Render/Vulkan/Renderer.h"
#include "Render/Vulkan/VulkanContext.h"

#include "Thread/ThreadManager.h"
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

  thread_manager = create_ref<ThreadManager>();

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

  if (!core.init(m_spec))
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
  for (const auto& system : system_manager->get_systems()) {
    system.second->set_dispatcher(&dispatcher);
    system.second->on_init();
  }
}

Application& Application::push_layer(Layer* layer) {
  layer_stack.push_layer(layer);
  layer->on_attach(dispatcher);

  return *this;
}

Application& Application::push_overlay(Layer* layer) {
  layer_stack.push_overlay(layer);
  layer->on_attach(dispatcher);

  return *this;
}

void Application::run() {
  std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
  while (is_running) {
    update_timestep();

    update_layers(timestep);
    system_manager->on_update();

    update_renderer();

    Window::poll_events();
    while (VulkanContext::get()->suspend) {
      Window::wait_for_events();
    }
  }

  system_manager->shutdown();
  core.shutdown();
}

void Application::update_layers(const Timestep& ts) {
  OX_SCOPED_ZONE_N("LayersLoop");
  for (Layer* layer : layer_stack)
    layer->on_update(ts);
}

void Application::update_renderer() {
  Renderer::draw(VulkanContext::get(), imgui_layer, layer_stack, system_manager);
}

void Application::update_timestep() {
  timestep.on_update();

  ImGuiIO& io = ImGui::GetIO();
  io.DeltaTime = (float)timestep.get_seconds();
}

void Application::close() {
  is_running = false;
}
}
