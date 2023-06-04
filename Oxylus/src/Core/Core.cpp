#include "src/oxpch.h"
#include "Core.h"
#include "Resources.h"
#include "Project.h"
#include "Audio/AudioEngine.h"
#include "Core/Input.h"

#include "Render/Window.h"
#include "Physics/Physics.h"
#include "Render/Vulkan/VulkanContext.h"
#include "Render/Vulkan/VulkanRenderer.h"
#include "Utils/UIUtils.h"

namespace Oxylus {
  Core::RenderBackend Core::s_Backend = RenderBackend::Vulkan;

  bool Core::Init(const AppSpec& spec) {
    ZoneScoped;
    if (!Resources::ResourcesPathExists()) {
      OX_CORE_FATAL("Resources path doesn't exists. Make sure the working directory is correct!");
      Application::Get().Close();
      return false;
    }

    FileDialogs::InitNFD();
    Project::New();
    Window::InitWindow(spec);
    VulkanContext::CreateContext(spec);
    VulkanRenderer::Init();
    Input::Init();
    AudioEngine::Init();

    return true;
  }

  void Core::Shutdown() {
    FileDialogs::CloseNFD();
    VulkanRenderer::WaitDeviceIdle();
    VulkanRenderer::Shutdown();
    AudioEngine::Shutdown();

    ThreadManager::Get()->WaitAllThreads();

    Window::CloseWindow(Window::GetGLFWWindow());
  }
}
