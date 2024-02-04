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
class AudioSource;

using AssetID = std::string;

class AssetManager {
public:
  // TODO(hatrickek): rethink about the use of this function
  static std::filesystem::path get_asset_file_system_path(const std::filesystem::path& path);

  static Shared<TextureAsset> get_texture_asset(const TextureLoadInfo& info);
  static Shared<TextureAsset> get_texture_asset(const std::string& name, const TextureLoadInfo& info);
  static Shared<Mesh> get_mesh_asset(const std::string& path, uint32_t loadingFlags = 0);
  static Shared<AudioSource> get_audio_asset(const std::string& path);

  static void package_assets();
  static void free_unused_assets();

private:
  static struct AssetLibrary {
    std::unordered_map<AssetID, Shared<TextureAsset>> texture_assets ;
    std::unordered_map<AssetID, Shared<Mesh>> mesh_assets;
    std::unordered_map<AssetID, Shared<AudioSource>> audio_assets;
  } asset_library;

  static Shared<TextureAsset> load_texture_asset(const std::string& path);
  static Shared<TextureAsset> load_texture_asset(const std::string& path, const TextureLoadInfo& info);
  static Shared<Mesh> load_mesh_asset(const std::string& path, uint32_t loadingFlags);
  static Shared<AudioSource> load_audio_asset(const std::string& path);
};
}
