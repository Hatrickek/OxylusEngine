#include "App.hpp"

#include <filesystem>

#include "FileSystem.hpp"
#include "Layer.hpp"
#include "LayerStack.hpp"
#include "Project.hpp"

#include "Audio/AudioEngine.hpp"

#include "Modules/ModuleRegistry.hpp"

#include "Render/Renderer.hpp"
#include "Render/Vulkan/VkContext.hpp"
#include "Render/Window.h"

#include "Scripting/LuaManager.hpp"

#include "Thread/TaskScheduler.hpp"
#include "Thread/ThreadManager.hpp"

#include "UI/ImGuiLayer.hpp"

#include "Utils/FileDialogs.hpp"
#include "Utils/Profiler.hpp"
#include "Utils/Random.hpp"

namespace ox {
App* App::instance = nullptr;

App::App(AppSpec spec) : app_spec(std::move(spec)) {
  OX_SCOPED_ZONE;
  if (instance) {
    OX_LOG_ERROR("Application already exists!");
    return;
  }

  instance = this;

  layer_stack = create_shared<LayerStack>();
  thread_manager = create_shared<ThreadManager>();

  if (app_spec.working_directory.empty())
    app_spec.working_directory = std::filesystem::current_path().string();
  else
    std::filesystem::current_path(app_spec.working_directory);

  for (int i = 0; i < app_spec.command_line_args.count; i++) {
    auto c = app_spec.command_line_args.args[i];
    if (!std::string(c).empty()) {
      command_line_args.emplace_back(c);
    }
  }

  if (!asset_directory_exists()) {
    OX_LOG_FATAL("Resources path doesn't exists. Make sure the working directory is correct! Editor should be launched in Oxylus/OxylusEditor.");
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

  Window::init_window(app_spec);
  Window::set_dispatcher(&dispatcher);
  Input::init();
  Input::set_dispatcher_events(dispatcher);

  for (auto& [_, system] : system_registry) {
    system->set_dispatcher(&dispatcher);
    system->init();
  }

  VkContext::init();
  VkContext::get()->create_context(app_spec);
  Renderer::init();

  imgui_layer = new ImGuiLayer();
  push_overlay(imgui_layer);
}

App::~App() { close(); }

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
    while (VkContext::get()->suspend) {
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

void App::update_renderer() { Renderer::draw(VkContext::get(), imgui_layer, *layer_stack.get()); }

void App::update_timestep() {
  timestep.on_update();

  ImGuiIO& io = ImGui::GetIO();
  io.DeltaTime = (float)timestep.get_seconds();
}

void App::close() { is_running = false; }

bool App::asset_directory_exists() { return std::filesystem::exists(get_asset_directory()); }

std::string App::get_asset_directory() {
  if (Project::get_active() && !Project::get_active()->get_config().asset_directory.empty())
    return Project::get_asset_directory();
  return instance->app_spec.assets_path;
}

std::string App::get_asset_directory(const std::string_view asset_path) { return FileSystem::append_paths(get_asset_directory(), asset_path); }

std::string App::get_asset_directory_absolute() {
  if (Project::get_active()) {
    const auto p = std::filesystem::absolute(Project::get_asset_directory());
    return p.string();
  }
  const auto p = absolute(std::filesystem::path(instance->app_spec.assets_path));
  return p.string();
}

std::string App::get_relative(const std::string& path) {
  return FileSystem::preferred_path(std::filesystem::relative(path, get_asset_directory()).string());
}

std::string App::get_absolute(const std::string& path) {
  return FileSystem::append_paths(FileSystem::preferred_path(get_asset_directory_absolute()), path);
}
} // namespace ox
