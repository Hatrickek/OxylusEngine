#include "Resources.h"

#include "Application.h"
#include "EmbeddedResources.h"

namespace Oxylus {
  Resources::EditorRes Resources::s_EditorResources;
  Resources::EngineRes Resources::s_EngineResources;

  void Resources::InitEditorResources() {
    VulkanImageDescription imageDescription;
    imageDescription.Format = vk::Format::eR8G8B8A8Unorm;
    imageDescription.Width = 40;
    imageDescription.Height = 40;
    imageDescription.CreateDescriptorSet = true;
    imageDescription.EmbeddedData = EngineLogo;
    imageDescription.EmbeddedDataLength = EngineLogoLen;
    s_EditorResources.EngineIcon.Create(imageDescription);
  }

  std::filesystem::path Resources::GetResourcesPath(const std::filesystem::path& path) {
    return Application::Get().Spec.WorkingDirectory / "Resources" / path;
  }

  bool Resources::ResourcesPathExists() {
    return std::filesystem::exists("Resources");
  }

  void Resources::InitEngineResources() {
    s_EngineResources.EmptyTexture = CreateRef<VulkanImage>();
    VulkanImageDescription emptyDescription{};
    emptyDescription.Width = 1;
    emptyDescription.Height = 1;
    emptyDescription.CreateDescriptorSet = true;
    s_EngineResources.EmptyTexture->Create(emptyDescription);
  }
}
