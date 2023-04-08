#include "src/oxpch.h"
#include "Resources.h"

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

  void Resources::InitEngineResources() {
    VulkanImageDescription emptyDescription{};
    emptyDescription.Width = 1;
    emptyDescription.Height = 1;
    emptyDescription.CreateDescriptorSet = true;
    s_EngineResources.EmptyTexture.Create(emptyDescription);
  }
}
