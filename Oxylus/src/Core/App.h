#pragma once
#include <string>

#include <ankerl/unordered_dense.h>

#include "Base.h"
#include "ESystem.h"

#include "Utils/Log.h"
#include "Utils/Timestep.h"

int main(int argc, char** argv);

namespace Ox {
class Layer;
class LayerStack;
class ImGuiLayer;
class ThreadManager;

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
  std::string assets_path = "Resources";
  uint32_t device_index = 0;
  ApplicationCommandLineArgs command_line_args;
};

using SystemRegistry = ankerl::unordered_dense::map<size_t, Shared<ESystem>>;

class App {
public:
  App(AppSpec spec);
  virtual ~App();

  App& push_layer(Layer* layer);
  App& push_overlay(Layer* layer);

  void close();

  const AppSpec& get_specification() const { return app_spec; }
  const std::vector<std::string>& get_command_line_args() const { return command_line_args; }
  ImGuiLayer* get_imgui_layer() const { return imgui_layer; }
  const Shared<LayerStack>& get_layer_stack() const { return layer_stack; }
  static App* get() { return instance; }
  static const Timestep& get_timestep() { return instance->timestep; }

  static bool asset_directory_exists();
  static std::string get_asset_directory();
  static std::string get_asset_directory(std::string_view asset_path); // appends the asset_path at the end
  static std::string get_asset_directory_absolute();
  static std::string get_asset_directory_absolute(std::string_view asset_path); // appends the asset_path at the end

  static SystemRegistry& get_system_registry() { return instance->system_registry; }

  template <typename T, typename... Args>
  static void register_system(Args&&... args) {
    auto typeName = typeid(T).hash_code();
    OX_CORE_ASSERT(!instance->system_registry.contains(typeName), "Registering system more than once.");

    Shared<T> system = create_shared<T>(std::forward<Args>(args)...);
    instance->system_registry.emplace(typeName, std::move(system));
  }

  template <typename T>
  static void unregister_system() {
    const auto typeName = typeid(T).hash_code();

    if (instance->system_registry.contains(typeName)) {
      instance->system_registry.erase(typeName);
    }
  }

  template <typename T>
  static T* get_system() {
    const auto typeName = typeid(T).hash_code();

    if (instance->system_registry.contains(typeName)) {
      return dynamic_cast<T*>(instance->system_registry[typeName].get());
    }

    return nullptr;
  }

  template <typename T>
  static T* has_system() {
    const auto typeName = typeid(T).hash_code();
    return instance->system_registry.contains(typeName);
  }

private:
  static App* instance;
  AppSpec app_spec;
  std::vector<std::string> command_line_args;
  ImGuiLayer* imgui_layer;
  Shared<LayerStack> layer_stack;

  SystemRegistry system_registry = {};
  EventDispatcher dispatcher;

  Shared<ThreadManager> thread_manager;
  Timestep timestep;

  bool is_running = true;
  float last_frame_time = 0.0f;

  void run();
  void update_layers(const Timestep& ts);
  void update_renderer();
  void update_timestep();

  friend int ::main(int argc, char** argv);
};

App* create_application(ApplicationCommandLineArgs args);
}
