#pragma once

#include "Assets/Assets.h"

#include <filesystem>
#include <vector>

namespace Oxylus {
  struct VulkanImageDescription;
  class VulkanImage;
  class Material;
  class Mesh;

  class AssetManager {
  public:
    struct AssetsLibrary {
      std::unordered_map<AssetHandle, Asset<Material>> MaterialAssets{};
      std::unordered_map<AssetHandle, Asset<Mesh>>      MeshAssets{};
      std::unordered_map<AssetHandle, Asset<VulkanImage>> ImageAssets{};
    };

    static std::filesystem::path GetAssetFileSystemPath(const std::filesystem::path& path);

    // Assumes the path already points to an existing asset file.
    static Asset<VulkanImage> GetImageAsset(const std::string& path);
    // Assumes the path already points to an existing asset file.
    static Asset<VulkanImage> GetImageAsset(const VulkanImageDescription& description);
    static Asset<VulkanImage> GetImageAsset(const AssetHandle handle) { return s_AssetsLibrary.ImageAssets[handle]; }
    // Assumes the path already points to an existing asset file.
    static Asset<Mesh> GetMeshAsset(const std::string& path, int32_t loadingFlags = 0);
    static Asset<Mesh> GetMeshAsset(const AssetHandle handle) { return s_AssetsLibrary.MeshAssets[handle]; }
    // Assumes the path already points to an existing asset file.
    static Asset<Material> GetMaterialAsset(const std::string& path);
    static Asset<Material> GetMaterialAsset(AssetHandle handle) { return s_AssetsLibrary.MaterialAssets[handle]; }

    static const AssetsLibrary& GetAssetLibrary() { return s_AssetsLibrary; }

    static void PackageAssets();
    static void FreeUnusedAssets();

  private:
    static Asset<VulkanImage> LoadImageAsset(const VulkanImageDescription& description);
    static Asset<Mesh> LoadMeshAsset(const std::string& path, int32_t loadingFlags);
    static Asset<Material> LoadMaterialAsset(const std::string& path);

    static AssetsLibrary s_AssetsLibrary;

    static std::mutex s_AssetMutex;
  };
}
