#pragma once

#include <filesystem>
#include <unordered_map>

#include "Core/Base.h"

namespace Oxylus {
class TextureAsset;
struct ImageCreateInfo;
class Material;
class Mesh;

class AssetManager {
public:
  using AssetID = std::string;

  static std::filesystem::path GetAssetFileSystemPath(const std::filesystem::path& path);

  static Ref<TextureAsset> GetTextureAsset(const std::string& path);
  static Ref<TextureAsset> GetTextureAsset(const std::string& name, void* initialData, size_t size);
  static Ref<Mesh> GetMeshAsset(const std::string& path, int32_t loadingFlags = 0);

  static void PackageAssets();
  static void FreeUnusedAssets();

private:
  static struct AssetLibrary {
    std::unordered_map<AssetID, Ref<TextureAsset>> m_TextureAssets = {};
    std::unordered_map<AssetID, Ref<Mesh>> m_MeshAssets = {};
  } s_Library;

  static Ref<TextureAsset> LoadTextureAsset(const std::string& path);
  static Ref<TextureAsset> LoadTextureAsset(const std::string& name, void* initialData, size_t size);
  static Ref<Mesh> LoadMeshAsset(const std::string& path, int32_t loadingFlags);

  static std::mutex s_AssetMutex;
};
}
