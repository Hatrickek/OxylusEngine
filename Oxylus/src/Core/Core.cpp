#include "src/oxpch.h"
#include "Core.h"

#include "Project.h"
#include "Audio/AudioEngine.h"
#include "Core/Input.h"

#include "Render/Window.h"
#include "Physics/Physics.h"
#include "Render/Vulkan/VulkanContext.h"
#include "Render/Vulkan/VulkanRenderer.h"

namespace Oxylus {
  Core::RenderBackend Core::s_Backend = RenderBackend::Vulkan;

  void Core::Init(const AppSpec& spec) {
    s_Backend = spec.Backend;
    Project::New();
    Window::InitWindow(spec);
    VulkanContext::CreateContext(spec);
    VulkanRenderer::Init();
    Input::Init();
    Physics::InitPhysics();
    AudioEngine::Init();
  }

  void Core::Shutdown() {
    VulkanRenderer::WaitDeviceIdle();
    VulkanRenderer::Shutdown();

    AudioEngine::Shutdown();

    ThreadManager::Get()->WaitAllThreads();

    Window::CloseWindow(Window::GetGLFWWindow());
  }
}
