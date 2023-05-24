#include "src/oxpch.h"
#include "AssetManager.h"

#include "MaterialSerializer.h"
#include "Core/Project.h"
#include "Render/Mesh.h"
#include "Utils/Profiler.h"

namespace Oxylus {
  AssetManager::AssetsLibrary AssetManager::s_AssetsLibrary;
  std::mutex AssetManager::s_AssetMutex;

  std::filesystem::path AssetManager::GetAssetFileSystemPath(const std::filesystem::path& path) {
    return Project::GetAssetDirectory() / path;
  }

  Asset<VulkanImage> AssetManager::GetImageAsset(const std::string& path) {
    ZoneScoped;
    for (auto& asset : s_AssetsLibrary.ImageAssets) {
      if (asset.Path == path) {
        return asset;
      }
    }

    VulkanImageDescription desc;
    desc.Path = path;
    desc.CreateDescriptorSet = true;
    return LoadImageAsset(desc);
  }

  Asset<VulkanImage> AssetManager::GetImageAsset(const VulkanImageDescription& description) {
    ZoneScoped;
    for (auto& asset : s_AssetsLibrary.ImageAssets) {
      if (asset.Path == description.Path) {
        return asset;
      }
    }
    return LoadImageAsset(description);
  }

  Asset<Mesh> AssetManager::GetMeshAsset(const std::string& path, const int32_t loadingFlags) {
    ZoneScoped;
    for (auto& asset : s_AssetsLibrary.MeshAssets) {
      if (asset.Path == path) {
        return asset;
      }
    }

    return LoadMeshAsset(path, loadingFlags);
  }

  Asset<Material> AssetManager::GetMaterialAsset(const std::string& path) {
    ZoneScoped;
    for (auto& asset : s_AssetsLibrary.MaterialAssets) {
      if (asset.Path == path) {
        return asset;
      }
    }

    return LoadMaterialAsset(path);
  }

  void AssetManager::FreeUnusedAssets() {
    ZoneScoped;
    for (auto& asset : s_AssetsLibrary.MeshAssets) {
      asset.Data.reset();
      asset.Path.clear();
      asset.Handle = {};
    }
    for (auto& asset : s_AssetsLibrary.MaterialAssets) {
      asset.Data.reset();
      asset.Path.clear();
      asset.Handle = {};
    }
    for (auto& asset : s_AssetsLibrary.ImageAssets) {
      asset.Data.reset();
      asset.Path.clear();
      asset.Handle = {};
    }
  }

  Asset<VulkanImage> AssetManager::LoadImageAsset(const VulkanImageDescription& description) {
    ZoneScoped;
    Asset<VulkanImage> asset;
    std::lock_guard lock(s_AssetMutex);
    asset.Data = CreateRef<VulkanImage>(description);
    asset.Path = description.Path;
    asset.Type = AssetType::Image;
    return s_AssetsLibrary.ImageAssets.emplace_back(asset);
  }

  Asset<Mesh> AssetManager::LoadMeshAsset(const std::string& path, int32_t loadingFlags) {
    ZoneScoped;
    Asset<Mesh> asset;
    asset.Data = CreateRef<Mesh>(path, loadingFlags);
    asset.Path = path;
    asset.Type = AssetType::Mesh;
    return s_AssetsLibrary.MeshAssets.emplace_back(asset);
  }

  Asset<Material> AssetManager::LoadMaterialAsset(const std::string& path) {
    ZoneScoped;
    Asset<Material> asset;
    asset.Data = CreateRef<Material>();
    asset.Data->Create();
    const MaterialSerializer serializer(*asset.Data);
    serializer.Deserialize(path);
    asset.Path = path;
    asset.Type = AssetType::Material;
    return s_AssetsLibrary.MaterialAssets.emplace_back(asset);
  }
}
