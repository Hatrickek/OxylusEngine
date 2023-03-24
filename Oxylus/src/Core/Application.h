#pragma once
#include "Core/Core.h"
#include "Layerstack.h"
#include "UI/ImGuiLayer.h"
#include <filesystem>

#include "Systems/System.h"
#include "Thread/ThreadManager.h"
#include "Utils/Log.h"

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
    bool UseImGui = true;
    bool isRunning;
    bool CustomWindowTitle = true;
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

    template <typename T, typename... Args>
    Application& AddSystem(Args&&... args) {
      m_Systems.emplace_back(CreateScope<T>(std::forward<Args>(args)...));
      return *this;
    }

    void Close();

    ImGuiLayer* GetImGuiLayer() const { return m_ImGuiLayer; }
    const LayerStack& GetLayerStack() const { return m_LayerStack; }
    static Application& Get() { return *s_Instance; }

  private:
    void Run();
    void UpdateLayers();
    void UpdateRenderer();
    void UpdateImGui();
    void UpdateSystems() const;
    ImGuiLayer* m_ImGuiLayer;
    LayerStack m_LayerStack;

    std::vector<Scope<System>> m_Systems;
    EventDispatcher m_Dispatcher;

    ThreadManager m_ThreadManager;

    bool m_IsRunning = true;
    static Application* s_Instance;
    friend int ::main(int argc, char** argv);
  };

  Application* CreateApplication(ApplicationCommandLineArgs args);
}
