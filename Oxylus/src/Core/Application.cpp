#include "src/oxpch.h"
#include "Application.h"
#include "Core.h"
#include "Layer.h"
#include "Render/Window.h"
#include "Render/Vulkan/VulkanRenderer.h"
#include "Utils/Profiler.h"
#include "Utils/TimeStep.h"

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
    for (const auto& system : m_Systems) {
      system->SetDispatcher(&m_Dispatcher);
      system->OnInit();
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
      Timestep::UpdateTime();

      //Layers
      UpdateLayers();

      //Render Loop
      UpdateRenderer();

      //ImGui Loop
      UpdateImGui();

      //Systems
      UpdateSystems();

      Window::UpdateWindow();

      FrameMark;
    }

    for (const auto& system : m_Systems) {
      system->OnShutdown();
    }
    Core::Shutdown();
  }

  void Application::UpdateLayers() {
    ZoneScopedN("LayersLoop");
    for (Layer* layer : m_LayerStack)
      layer->OnUpdate(Timestep::GetDeltaTime());
  }

  void Application::UpdateRenderer() {
    ZoneScopedN("RenderLoop");
    VulkanRenderer::Draw();
  }

  void Application::UpdateImGui() {
    ZoneScopedN("ImGuiLoop");
    if (this->Spec.UseImGui) {
      m_ImGuiLayer->Begin();
      {
        ZoneScopedN("LayerImGuiRender");
        for (Layer* layer : m_LayerStack)
          layer->OnImGuiRender();
      }
      {
        ZoneScopedN("SystemImGuiRender");
        for (const auto& system : m_Systems)
          system->OnImGuiRender();
      }
      m_ImGuiLayer->End();
    }
  }

  void Application::UpdateSystems() const {
    for (const auto& system : m_Systems) {
      system->OnUpdate();
    }
  }

  void Application::Close() {
    m_IsRunning = false;
  }
}
