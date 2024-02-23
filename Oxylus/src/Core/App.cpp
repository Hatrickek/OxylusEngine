#include "App.h"

#include <filesystem>

#include "Layer.h"
#include "LayerStack.h"
#include "Project.h"
#include "Resources.h"

#include "Audio/AudioEngine.h"

#include "Modules/ModuleRegistry.h"

#include "Render/Window.h"
#include "Render/Vulkan/Renderer.h"
#include "Render/Vulkan/VulkanContext.h"

#include "Scripting/LuaManager.h"

#include "Thread/TaskScheduler.h"
#include "Thread/ThreadManager.h"

#include "UI/ImGuiLayer.h"

#include "Utils/FileDialogs.h"
#include "Utils/Profiler.h"
#include "Utils/Random.h"

namespace Ox {
App* App::instance = nullptr;

App::App(AppSpec spec) : m_spec(std::move(spec)) {
  OX_SCOPED_ZONE;
  if (instance) {
    OX_CORE_ERROR("Application already exists!");
    return;
  }

  instance = this;

  layer_stack = create_shared<LayerStack>();
  thread_manager = create_shared<ThreadManager>();

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

  if (!Resources::resources_path_exists()) {
    OX_CORE_ERROR("Resources path doesn't exists. Make sure the working directory is correct!");
    close();
    return;
  }

  register_system<Random>();
  register_system<TaskScheduler>();
  register_system<FileDialogs>();
  register_system<AudioEngine>();
  register_system<LuaManager>();
  register_system<ModuleRegistry>();
  register_system<RendererConfig>();

  Window::init_window(m_spec);
  Window::set_dispatcher(&dispatcher);
  Input::init();
  Input::set_dispatcher_events(dispatcher);

  for (auto& [_, system] : system_registry) {
    system->set_dispatcher(&dispatcher);
    system->init();
  }

  VulkanContext::init();
  VulkanContext::get()->create_context(m_spec);
  Renderer::init();

  imgui_layer = create_unique<ImGuiLayer>();
  push_overlay(imgui_layer.get());
}

App::~App() {
  close();
}

App& App::push_layer(Layer* layer) {
  layer_stack->push_layer(layer);
  layer->on_attach(dispatcher);

  return *this;
}

App& App::push_overlay(Layer* layer) {
  layer_stack->push_overlay(layer);
  layer->on_attach(dispatcher);

  return *this;
}

void App::run() {
  while (is_running) {
    update_timestep();

    update_layers(timestep);

    for (auto& [_, system] : system_registry)
      system->update();

    update_renderer();

    Input::reset_pressed();

    Window::poll_events();
    while (VulkanContext::get()->suspend) {
      Window::wait_for_events();
    }
  }

  layer_stack.reset();

  if (Project::get_active())
    Project::get_active()->unload_module();

  for (auto& [_, system] : system_registry)
    system->deinit();

  Renderer::deinit();
  ThreadManager::get()->wait_all_threads();
  Window::close_window(Window::get_glfw_window());
}

void App::update_layers(const Timestep& ts) {
  OX_SCOPED_ZONE_N("LayersLoop");
  for (Layer* layer : *layer_stack.get())
    layer->on_update(ts);
}

void App::update_renderer() {
  Renderer::draw(VulkanContext::get(), imgui_layer.get(), *layer_stack.get());
}

void App::update_timestep() {
  timestep.on_update();

  ImGuiIO& io = ImGui::GetIO();
  io.DeltaTime = (float)timestep.get_seconds();
}

void App::close() {
  is_running = false;
}

std::string App::get_asset_directory() {
  if (Project::get_active())
    return Project::get_asset_directory();
  return get_resources_path();
}

std::string App::get_asset_directory(const std::string_view asset_path) {
  return get_asset_directory().append("/").append(asset_path);
}

std::string App::get_asset_directory_absolute() {
  if (Project::get_active()) {
    const auto p = std::filesystem::absolute(Project::get_asset_directory());
    return p.string();
  }
  const auto p = absolute(std::filesystem::path(get_resources_path()));
  return p.string();
}

std::string App::get_asset_directory_absolute(std::string_view asset_path) {
  return FileSystem::append_paths(get_asset_directory_absolute(), asset_path);
}
}
