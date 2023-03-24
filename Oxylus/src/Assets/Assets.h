#pragma once

#include <oxpch.h>

#include "Core/UUID.h"
#include "Render/Vulkan/VulkanImage.h"

#include "Core/Base.h"

namespace Oxylus {
  struct VulkanImageDescription;
  class VulkanImage;
  class Mesh;

  enum class AssetType {
    None = 0,
    Mesh,
    Sound,
    Image,
    Material
  };

  struct AssetHandle {
    UUID ID;

    bool operator==(const AssetHandle& other) const {
      return ID == other.ID;
    }
  };

  template<typename T>
  struct Asset {
    Ref<T> Data = nullptr;
    AssetHandle Handle;
    std::string Path;
    AssetType Type{};

    explicit operator bool() const {
      return !Path.empty();
    }
  };

  class VulkanImage;

  struct EditorAsset : Asset<VulkanImage> { };
}
