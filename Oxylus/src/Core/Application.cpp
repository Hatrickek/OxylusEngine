#include "Application.h"
#include "Core.h"
#include "Layer.h"
#include "Render/Window.h"
#include "Render/Vulkan/VulkanRenderer.h"
#include "Utils/Profiler.h"

namespace Oxylus {
  Application* Application::s_Instance = nullptr;

  Application::Application(const AppSpec& spec) : Spec(spec) {
    if (s_Instance) {
      OX_CORE_ERROR("Application already exists!");
      return;
    }

    s_Instance = this;

    if (!Core::Init(spec))
      return;

    if (spec.UseImGui) {
      m_ImGuiLayer = new ImGuiLayer();
      PushOverlay(m_ImGuiLayer);
    }
  }

  Application::~Application() {
    Close();
  }

  void Application::InitSystems() {
    for (const auto& system : m_SystemManager.GetSystems()) {
      system.second->SetDispatcher(&m_Dispatcher);
      system.second->OnInit();
    }
  }

  Application& Application::PushLayer(Layer* layer) {
    m_LayerStack.PushLayer(layer);
    layer->OnAttach(m_Dispatcher);

    return *this;
  }

  Application& Application::PushOverlay(Layer* layer) {
    m_LayerStack.PushOverlay(layer);
    layer->OnAttach(m_Dispatcher);

    return *this;
  }

  void Application::Run() {
    while (m_IsRunning) {
      const auto time = static_cast<float>(glfwGetTime());
      m_Timestep = time - m_LastFrameTime;
      m_LastFrameTime = time;

      //Layers
      UpdateLayers(m_Timestep);

      //Render Loop
      UpdateRenderer();

      //ImGui Loop
      UpdateImGui();

      //Systems
      m_SystemManager.OnUpdate();

      Window::UpdateWindow();

      FrameMark;
    }

    m_SystemManager.Shutdown();
    Core::Shutdown();
  }

  void Application::UpdateLayers(Timestep ts) {
    OX_SCOPED_ZONE_N("LayersLoop");
    for (Layer* layer : m_LayerStack)
      layer->OnUpdate(ts);
  }

  void Application::UpdateRenderer() {
    OX_SCOPED_ZONE_N("RenderLoop");
    VulkanRenderer::Draw();
  }

  void Application::UpdateImGui() {
    OX_SCOPED_ZONE_N("ImGuiLoop");
    if (this->Spec.UseImGui) {
      m_ImGuiLayer->Begin();
      {
        OX_SCOPED_ZONE_N("LayerImGuiRender");
        for (Layer* layer : m_LayerStack)
          layer->OnImGuiRender();
      }
      {
        OX_SCOPED_ZONE_N("SystemImGuiRender");
        m_SystemManager.OnImGuiRender();
      }
      m_ImGuiLayer->End();
    }
  }

  void Application::Close() {
    m_IsRunning = false;
  }
}
