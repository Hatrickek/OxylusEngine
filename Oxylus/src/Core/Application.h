#pragma once
#include <string>

#include "Core.h"
#include "Layer.h"
#include "LayerStack.h"
#include "Thread/ThreadManager.h"
#include "UI/ImGuiLayer.h"
#include "Utils/Log.h"

int main(int argc, char** argv);

namespace Oxylus {
class SystemManager;

struct ApplicationCommandLineArgs {
  int count = 0;
  char** args = nullptr;

  const char* operator[](int index) const {
    OX_CORE_ASSERT(index < count)
    return args[index];
  }
};

struct AppSpec {
  std::string name = "Oxylus App";
  std::string working_directory = {};
  std::string resources_path = "Resources";
  bool custom_window_title = false;
  uint32_t device_index = 0;
  ApplicationCommandLineArgs command_line_args;
};

class Application {
public:
  Application(AppSpec spec);
  virtual ~Application();

  void init_systems();

  Application& push_layer(Layer* layer);
  Application& push_overlay(Layer* layer);

  Ref<SystemManager> get_system_manager() { return system_manager; }

  void close();

  const AppSpec& get_specification() const { return m_spec; }
  const std::vector<std::string>& get_command_line_args() const { return command_line_args; }
  ImGuiLayer* get_imgui_layer() const { return imgui_layer; }
  const LayerStack& get_layer_stack() const { return layer_stack; }
  static Application* get() { return instance; }
  static Timestep get_timestep() { return instance->timestep; }

private:
  AppSpec m_spec;
  std::vector<std::string> command_line_args;
  Core core;
  ImGuiLayer* imgui_layer;
  LayerStack layer_stack;

  Ref<SystemManager> system_manager = nullptr;
  EventDispatcher dispatcher;

  ThreadManager thread_manager;
  Timestep timestep;

  bool is_running = true;
  float last_frame_time = 0.0f;
  static Application* instance;

  void run();
  void update_layers(Timestep ts);
  void update_renderer();

  friend int ::main(int argc, char** argv);
};

Application* create_application(ApplicationCommandLineArgs args);
}
