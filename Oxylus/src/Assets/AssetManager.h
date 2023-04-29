#pragma once

#include "Assets/Assets.h"

#include <filesystem>

namespace Oxylus {
  class VulkanImage;
  class Material;
  class Mesh;

  class AssetManager {
  public:
    static std::filesystem::path GetAssetFileSystemPath(const std::filesystem::path& path);

    // Assumes the path already points to an existing asset file.
    static const Asset<VulkanImage>& GetImageAsset(const std::string& path);
    // Assumes the path already points to an existing asset file.
    static const Asset<VulkanImage>& GetImageAsset(const VulkanImageDescription& description);
    // Assumes the path already points to an existing asset file.
    static const Asset<Mesh>& GetMeshAsset(const std::string& path, int32_t loadingFlags = 0);
    // Assumes the path already points to an existing asset file.
    static const Asset<Material>& GetMaterialAsset(const std::string& path); 

    static void FreeUnusedAssets();

  private:
    static const Asset<VulkanImage>& LoadImageAsset(const VulkanImageDescription& description);
    static const Asset<Mesh>& LoadMeshAsset(const std::string& path, int32_t loadingFlags);
    static const Asset<Material>& LoadMaterialAsset(const std::string& path);

    static struct AssetsLibrary {
      std::vector<Asset<Material>> MaterialAssets{};
      std::vector<Asset<Mesh>> MeshAssets{};
      std::vector<Asset<VulkanImage>> ImageAssets{};
    } s_AssetsLibrary;

    static std::mutex s_AssetMutex;
  };
}
