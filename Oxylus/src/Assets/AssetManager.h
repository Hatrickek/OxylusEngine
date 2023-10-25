#pragma once

#include <filesystem>
#include <unordered_map>

#include "Core/Base.h"

namespace Oxylus {
struct TextureLoadInfo;
class TextureAsset;
struct ImageCreateInfo;
class Material;
class Mesh;

using AssetID = std::string;

class AssetManager {
public:
  static std::filesystem::path get_asset_file_system_path(const std::filesystem::path& path);

  static Ref<TextureAsset> get_texture_asset(const TextureLoadInfo& info);
  static Ref<TextureAsset> get_texture_asset(const std::string& name, const TextureLoadInfo& info);
  static Ref<Mesh> get_mesh_asset(const std::string& path, int32_t loadingFlags = 0);

  static void package_assets();
  static void free_unused_assets();

private:
  static struct AssetLibrary {
    std::unordered_map<AssetID, Ref<TextureAsset>> texture_assets = {};
    std::unordered_map<AssetID, Ref<Mesh>> mesh_assets = {};
  } s_library;

  static Ref<TextureAsset> load_texture_asset(const std::string& path);
  static Ref<TextureAsset> load_texture_asset(const std::string& path, const TextureLoadInfo& info);
  static Ref<Mesh> load_mesh_asset(const std::string& path, int32_t loadingFlags);

  static std::mutex s_asset_mutex;
};
}
