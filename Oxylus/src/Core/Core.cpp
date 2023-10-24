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
  VulkanRenderer::init();

  Input::Init();
  AudioEngine::Init();

  return true;
}

void Core::shutdown() {
  FileDialogs::CloseNFD();

  VulkanRenderer::shutdown();

  AudioEngine::Shutdown();

  ThreadManager::get()->wait_all_threads();

  Window::close_window(Window::get_glfw_window());
}
}
