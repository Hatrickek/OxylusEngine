#pragma once

#include "Core/Core.h"
#include "LayerStack.h"
#include "UI/ImGuiLayer.h"
#include <filesystem>

#include "Systems/System.h"
#include "Systems/SystemManager.h"
#include "Thread/ThreadManager.h"
#include "Utils/Log.h"
#include "Utils/Timestep.h"

int main(int argc,
         char** argv);

namespace Oxylus {
  struct ApplicationCommandLineArgs {
    int Count = 0;
    char** Args = nullptr;

    const char* operator[](int index) const {
      OX_CORE_ASSERT(index < Count);
      return Args[index];
    }
  };

  struct AppSpec {
    std::string Name = "Oxylus App";
    Core::RenderBackend Backend = Core::RenderBackend::Vulkan;
    std::filesystem::path WorkingDirectory = std::filesystem::current_path();
    std::string ResourcesPath = "Resources";
    bool UseImGui = true;
    bool CustomWindowTitle = false;
    ApplicationCommandLineArgs CommandLineArgs;
  };

  class Application {
  public:
    AppSpec Spec;

    Application(const AppSpec& spec);
    virtual ~Application();

    void InitSystems();

    Application& PushLayer(Layer* layer);
    Application& PushOverlay(Layer* layer);

    SystemManager& GetSystemManager() { return m_SystemManager; }

    void Close();

    ImGuiLayer* GetImGuiLayer() const { return m_ImGuiLayer; }
    const LayerStack& GetLayerStack() const { return m_LayerStack; }
    static Application* Get() { return s_Instance; }
    static Timestep GetTimestep() { return s_Instance->m_Timestep; }

  private:
    void Run();
    void UpdateLayers(Timestep ts);
    void UpdateRenderer();
    void UpdateImGui();

    ImGuiLayer* m_ImGuiLayer;
    LayerStack m_LayerStack;

    SystemManager m_SystemManager;
    EventDispatcher m_Dispatcher;

    ThreadManager m_ThreadManager;
    Timestep m_Timestep;

    bool m_IsRunning = true;
    float m_LastFrameTime = 0.0f;
    static Application* s_Instance;

    friend int ::main(int argc, char** argv);
  };

  Application* CreateApplication(ApplicationCommandLineArgs args);
}
