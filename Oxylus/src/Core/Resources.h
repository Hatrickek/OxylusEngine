#pragma once

#include <filesystem>

#include "Render/Vulkan/VulkanImage.h"

namespace Oxylus {
  
  class Resources {
  public:
    static struct EditorRes {
      VulkanImage EngineIcon;
    } s_EditorResources;

    static struct EngineRes {
      Ref<VulkanImage> EmptyTexture = nullptr;
      VulkanImage CheckboardTexture;
    } s_EngineResources;

    static void InitEngineResources();
    static void InitEditorResources();

    static std::string GetResourcesPath(const std::filesystem::path& path);
    static bool ResourcesPathExists();
  };
}
