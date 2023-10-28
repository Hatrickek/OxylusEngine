#include "Core.h"
#include "Project.h"
#include "Resources.h"
#include "Audio/AudioEngine.h"
#include "Core/Input.h"

#include "Physics/Physics.h"
#include "Render/Window.h"

#include "Render/Vulkan/Renderer.h"
#include "Render/Vulkan/VulkanContext.h"

#include "Thread/ThreadManager.h"

#include "Utils/Profiler.h"

#include "Utils/UIUtils.h"

namespace Oxylus {
bool Core::init(const AppSpec& spec) {
  OX_SCOPED_ZONE;
  if (!Resources::resources_path_exists()) {
    OX_CORE_FATAL("Resources path doesn't exists. Make sure the working directory is correct!");
    Application::get()->close();
    return false;
  }

  FileDialogs::InitNFD();
  Project::New();
  Window::init_window(spec);

  VulkanContext::init();
  VulkanContext::get()->create_context(spec);
  Renderer::init();

  Input::Init();
  AudioEngine::Init();

  return true;
}

void Core::shutdown() {
  FileDialogs::CloseNFD();

  Renderer::shutdown();

  AudioEngine::Shutdown();

  ThreadManager::get()->wait_all_threads();

  Window::close_window(Window::get_glfw_window());
}
}
