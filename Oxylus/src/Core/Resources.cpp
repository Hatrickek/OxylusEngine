#include "Resources.h"

#include <filesystem>

#include "Application.h"
#include "EmbeddedLogo.h"
#include "Assets/TextureAsset.h"

namespace Oxylus {
Resources::EditorRes Resources::editor_resources;

void Resources::init_editor_resources() {
  editor_resources.engine_icon = create_ref<TextureAsset>();
  editor_resources.engine_icon->load_from_memory(EngineLogo, EngineLogoLen);
}

std::string Resources::get_resources_path(const std::string& path) {
  return (std::filesystem::path(Application::get_resources_path()) / path).string();
}

bool Resources::resources_path_exists() {
  return std::filesystem::exists(Application::get_resources_path());
}
}
