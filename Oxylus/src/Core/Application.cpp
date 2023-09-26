#include "Application.h"

#include <filesystem>

#include "Core.h"
#include "Layer.h"
#include "Render/Window.h"
#include "Render/Vulkan/VulkanContext.h"
#include "Render/Vulkan/VulkanRenderer.h"
#include "Systems/SystemManager.h"
#include "Utils/Profiler.h"

namespace Oxylus {
Application* Application::s_Instance = nullptr;

Application::Application(const AppSpec& spec) : m_Spec(spec), m_SystemManager(CreateRef<SystemManager>()) {
  if (s_Instance) {
    OX_CORE_ERROR("Application already exists!");
    return;
  }

  s_Instance = this;

  if (m_Spec.WorkingDirectory.empty())
    m_Spec.WorkingDirectory = std::filesystem::current_path().string();
  else
    std::filesystem::current_path(m_Spec.WorkingDirectory);

  for (int i = 0; i < m_Spec.CommandLineArgs.Count; i++) {
    auto c = m_Spec.CommandLineArgs.Args[i];
    if (!std::string(c).empty()) {
      m_CommandLineArgs.emplace_back(c);
    }
  }

  if (!m_Core.Init(m_Spec))
    return;

  Window::SetWindowUserData(this);

  glfwSetKeyCallback(Window::GetGLFWWindow(),
    [](GLFWwindow* window, int key, int scancode, int action, int mods) {
      const auto app = (Application*)glfwGetWindowUserPointer(window);

      if (action == GLFW_PRESS) {
        for (const auto& layer : app->m_LayerStack)
          layer->OnKeyPressed((KeyCode)key);
      }
      else if (action == GLFW_RELEASE) {
        for (const auto& layer : app->m_LayerStack)
          layer->OnKeyReleased((KeyCode)key);
      }
    });

  m_ImGuiLayer = new ImGuiLayer();
  PushOverlay(m_ImGuiLayer);
}

Application::~Application() {
  Close();
}

void Application::InitSystems() {
  for (const auto& system : m_SystemManager->GetSystems()) {
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
    Window::UpdateWindow();

    while (VulkanContext::Get()->Suspend) {
      Window::WaitForEvents();
    }

    const auto time = static_cast<float>(glfwGetTime());
    m_Timestep = time - m_LastFrameTime;
    m_LastFrameTime = time;

    // Layers
    UpdateLayers(m_Timestep);

    // Systems
    m_SystemManager->OnUpdate();

    if (!m_IsRunning)
      break;

    UpdateRenderer();

    FrameMark;
  }

  m_SystemManager->Shutdown();
  m_Core.Shutdown();
}

void Application::UpdateLayers(Timestep ts) {
  OX_SCOPED_ZONE_N("LayersLoop");
  for (Layer* layer : m_LayerStack)
    layer->OnUpdate(ts);
}

void Application::UpdateRenderer() {
  OX_SCOPED_ZONE_N("SwapchainLoop");
  VulkanRenderer::Draw(VulkanContext::Get(), m_ImGuiLayer, m_LayerStack, m_SystemManager);
}

void Application::Close() {
  m_IsRunning = false;
}
}
