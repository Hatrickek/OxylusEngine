#pragma once
#include <string>

#include "Core.h"
#include "Layer.h"
#include "LayerStack.h"
#include "Thread/ThreadManager.h"
#include "UI/ImGuiLayer.h"
#include "Utils/Log.h"

int main(int argc,
         char** argv);

namespace Oxylus {
class SystemManager;

struct ApplicationCommandLineArgs {
  int Count = 0;
  char** Args = nullptr;

  const char* operator[](int index) const {
    OX_CORE_ASSERT(index < Count)
    return Args[index];
  }
};

struct AppSpec {
  std::string Name = "Oxylus App";
  std::string WorkingDirectory = {};
  std::string ResourcesPath = "Resources";
  bool UseImGui = true;
  bool CustomWindowTitle = false;
  uint32_t DeviceIndex = 0;
  ApplicationCommandLineArgs CommandLineArgs;
};

class Application {
public:
  Application(const AppSpec& spec);
  virtual ~Application();

  void InitSystems();

  Application& PushLayer(Layer* layer);
  Application& PushOverlay(Layer* layer);

  Ref<SystemManager> GetSystemManager() { return m_SystemManager; }

  void Close();

  const AppSpec& GetSpecification() const { return m_Spec; }
  const std::vector<std::string>& GetCommandLineArgs() const { return m_CommandLineArgs; }
  ImGuiLayer* GetImGuiLayer() const { return m_ImGuiLayer; }
  const LayerStack& GetLayerStack() const { return m_LayerStack; }
  static Application* Get() { return s_Instance; }
  static Timestep GetTimestep() { return s_Instance->m_Timestep; }

private:
  AppSpec m_Spec;
  std::vector<std::string> m_CommandLineArgs;
  Core m_Core;
  ImGuiLayer* m_ImGuiLayer;
  LayerStack m_LayerStack;

  Ref<SystemManager> m_SystemManager = nullptr;
  EventDispatcher m_Dispatcher;

  ThreadManager m_ThreadManager;
  Timestep m_Timestep;

  bool m_IsRunning = true;
  float m_LastFrameTime = 0.0f;
  static Application* s_Instance;

  void Run();
  void UpdateLayers(Timestep ts);
  void UpdateRenderer();

  friend int ::main(int argc, char** argv);
};

Application* CreateApplication(ApplicationCommandLineArgs args);
}
