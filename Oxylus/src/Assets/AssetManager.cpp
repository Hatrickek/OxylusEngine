#include "AssetManager.h"

#include "MaterialSerializer.h"
#include "Core/Project.h"
#include "Render/Mesh.h"
#include "Render/ShaderLibrary.h"

#include "Utils/FileUtils.h"
#include "Utils/Profiler.h"

namespace Oxylus {
  AssetManager::AssetsLibrary AssetManager::s_AssetsLibrary;
  std::mutex AssetManager::s_AssetMutex;

  std::filesystem::path AssetManager::GetAssetFileSystemPath(const std::filesystem::path& path) {
    return Project::GetAssetDirectory() / path;
  }

  Asset<VulkanImage> AssetManager::GetImageAsset(const std::string& path) {
    OX_SCOPED_ZONE;
    for (auto& [handle, asset] : s_AssetsLibrary.ImageAssets) {
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
    OX_SCOPED_ZONE;
    for (auto& [handle, asset] : s_AssetsLibrary.ImageAssets) {
      if (asset.Path == description.Path) {
        return asset;
      }
    }

    return LoadImageAsset(description);
  }

  Asset<Mesh> AssetManager::GetMeshAsset(const std::string& path, const int32_t loadingFlags) {
    OX_SCOPED_ZONE;
    for (auto& [handle, asset] : s_AssetsLibrary.MeshAssets) {
      if (asset.Path == path) {
        return asset;
      }
    }

    return LoadMeshAsset(path, loadingFlags);
  }

  Asset<Material> AssetManager::GetMaterialAsset(const std::string& path) {
    OX_SCOPED_ZONE;
    for (auto& [handle, asset] : s_AssetsLibrary.MaterialAssets) {
      if (asset.Path == path) {
        return asset;
      }
    }

    return LoadMaterialAsset(path);
  }

  void AssetManager::PackageAssets() {
    FreeUnusedAssets();

    // TODO(hatrickek): Pack materials inside of meshes(glb).
    // TODO(hatrickek): Pack/compress audio files.
    // TODO(hatrickek): Convert all png etc. files to KTX files.

    constexpr auto meshDirectory = "Assets/Objects";
    constexpr auto textureDirectory = "Assets/Textures";
    constexpr auto soundsDirectory = "Assets/Sounds";
    constexpr auto scenesDirectory = "Assets/Scenes";

    std::filesystem::create_directory("Assets");

    // Package mesh files
    for (auto& [handle, asset] : s_AssetsLibrary.MeshAssets) {
      const auto filePath = std::filesystem::path(asset.Path);
      auto outPath = std::filesystem::path(meshDirectory) / filePath.filename();
      outPath = FileUtils::GetPreferredPath(outPath.string());
      auto dir = FileUtils::GetPreferredPath(meshDirectory);
      std::filesystem::create_directory(dir);
      const auto exported = Mesh::ExportAsBinary(asset.Path, outPath.string());
      if (!exported)
        OX_CORE_ERROR("Couldn't export mesh asset: {}", asset.Path);
      else
        OX_CORE_INFO("Exported mesh asset to: {}", outPath.string());
    }

    // Package texture files
    for (auto& [handle, asset] : s_AssetsLibrary.ImageAssets) {
      const auto filePath = std::filesystem::path(asset.Path);
      const auto outPath = std::filesystem::path(textureDirectory) / filePath.filename();
      std::filesystem::create_directory(textureDirectory);
      const auto exported = std::filesystem::copy_file(asset.Path, outPath);
      if (!exported)
        OX_CORE_ERROR("Couldn't export image asset: {}", asset.Path);
      else
        OX_CORE_INFO("Exported image asset to: {}", outPath.string());
    }
  }

  void AssetManager::FreeUnusedAssets() {
    OX_SCOPED_ZONE;
    for (auto& [handle, asset] : s_AssetsLibrary.MeshAssets) {
      if (asset.Data.use_count())
        return;
      s_AssetsLibrary.MeshAssets.erase(handle);
    }
    for (auto& [handle, asset] : s_AssetsLibrary.MaterialAssets) {
      if (asset.Data.use_count())
        return;
      s_AssetsLibrary.MaterialAssets.erase(handle);
    }
    for (auto& [handle, asset] : s_AssetsLibrary.ImageAssets) {
      if (asset.Data.use_count())
        return;
      s_AssetsLibrary.ImageAssets.erase(handle);
    }
  }

  Asset<VulkanImage> AssetManager::LoadImageAsset(const VulkanImageDescription& description) {
    OX_SCOPED_ZONE;
    Asset<VulkanImage> asset;
    std::lock_guard lock(s_AssetMutex);
    asset.Data = CreateRef<VulkanImage>(description);
    asset.Path = description.Path;
    asset.Type = AssetType::Image;
    return s_AssetsLibrary.ImageAssets.emplace(asset.Handle, asset).first->second;
  }

  Asset<Mesh> AssetManager::LoadMeshAsset(const std::string& path, int32_t loadingFlags) {
    OX_SCOPED_ZONE;
    Asset<Mesh> asset;
    asset.Data = CreateRef<Mesh>(path, loadingFlags);
    asset.Path = path;
    asset.Type = AssetType::Mesh;
    return s_AssetsLibrary.MeshAssets.emplace(asset.Handle, asset).first->second;
  }

  Asset<Material> AssetManager::LoadMaterialAsset(const std::string& path) {
    OX_SCOPED_ZONE;
    Asset<Material> asset;
    asset.Data = CreateRef<Material>();
    asset.Data->Create();
    const MaterialSerializer serializer(*asset.Data);
    serializer.Deserialize(path);
    asset.Path = path;
    asset.Type = AssetType::Material;
    return s_AssetsLibrary.MaterialAssets.emplace(asset.Handle, asset).first->second;
  }
}
