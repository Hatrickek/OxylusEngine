#include "Core.h"

#include "Project.h"
#include "Resources.h"

#include "Audio/AudioEngine.h"

#include "Core/Input.h"

#include "Physics/Physics.h"

#include "Render/Window.h"
#include "Render/Vulkan/Renderer.h"
#include "Render/Vulkan/VulkanContext.h"

#include "Scripting/LuaManager.h"

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

  FileDialogs::init_nfd();
  Project::create_new();
  Window::init_window(spec);

  VulkanContext::init();
  VulkanContext::get()->create_context(spec);
  Renderer::init();

  Input::init();
  AudioEngine::init();

  LuaManager::get()->init();

  return true;
}

void Core::shutdown() {
  FileDialogs::close_nfd();

  Renderer::shutdown();

  AudioEngine::shutdown();

  ThreadManager::get()->wait_all_threads();

  Window::close_window(Window::get_glfw_window());
}
}
