#include "src/oxpch.h"
#include "Core.h"

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

  void Core::Init(const AppSpec& spec) {
    s_Backend = spec.Backend;
    FileDialogs::InitNFD();
    Project::New();
    Window::InitWindow(spec);
    VulkanContext::CreateContext(spec);
    VulkanRenderer::Init();
    Input::Init();
    AudioEngine::Init();
    Physics::InitPhysics();
  }

  void Core::Shutdown() {
    FileDialogs::CloseNFD();
    VulkanRenderer::WaitDeviceIdle();
    VulkanRenderer::Shutdown();
    AudioEngine::Shutdown();
    Physics::ShutdownPhysics();

    ThreadManager::Get()->WaitAllThreads();

    Window::CloseWindow(Window::GetGLFWWindow());
  }
}
