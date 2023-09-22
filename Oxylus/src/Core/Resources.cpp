#include "Resources.h"

#include <filesystem>

#include "Application.h"
#include "EmbeddedLogo.h"
#include "Assets/TextureAsset.h"

namespace Oxylus {
Resources::EditorRes Resources::s_EditorResources;

void Resources::InitEditorResources() {
  s_EditorResources.EngineIcon = CreateRef<TextureAsset>();
  s_EditorResources.EngineIcon->LoadFromMemory(EngineLogo, EngineLogoLen);
}

std::string Resources::GetResourcesPath(const std::string& path) {
  const auto& spec = Application::Get()->GetSpecification();
  return (std::filesystem::path(spec.WorkingDirectory) / spec.ResourcesPath / path).string();
}

bool Resources::ResourcesPathExists() {
  return std::filesystem::exists(Application::Get()->GetSpecification().ResourcesPath);
}
}
