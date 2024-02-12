#include "AssetManager.h"

#include "Audio/AudioSource.h"
#include "Core/Project.h"
#include "Render/Mesh.h"

#include "Utils/Log.h"
#include "Utils/Profiler.h"

namespace Ox {
AssetManager::AssetLibrary AssetManager::asset_library;

Shared<TextureAsset> AssetManager::get_texture_asset(const TextureLoadInfo& info) {
  if (asset_library.texture_assets.contains(info.path)) {
    return asset_library.texture_assets[info.path];
  }

  return load_texture_asset(info.path, info);
}

Shared<TextureAsset> AssetManager::get_texture_asset(const std::string& name, const TextureLoadInfo& info) {
  if (asset_library.texture_assets.contains(name)) {
    return asset_library.texture_assets[name];
  }

  return load_texture_asset(name, info);
}

Shared<Mesh> AssetManager::get_mesh_asset(const std::string& path, const uint32_t loadingFlags) {
  OX_SCOPED_ZONE;
  if (asset_library.mesh_assets.contains(path)) {
    return asset_library.mesh_assets[path];
  }

  return load_mesh_asset(path, loadingFlags);
}

Shared<AudioSource> AssetManager::get_audio_asset(const std::string& path) {
  OX_SCOPED_ZONE;
  if (asset_library.audio_assets.contains(path)) {
    return asset_library.audio_assets[path];
  }

  return load_audio_asset(path);
}

Shared<TextureAsset> AssetManager::load_texture_asset(const std::string& path) {
  OX_SCOPED_ZONE;

  Shared<TextureAsset> texture = create_shared<TextureAsset>(path);
  return asset_library.texture_assets.emplace(path, texture).first->second;
}

Shared<TextureAsset> AssetManager::load_texture_asset(const std::string& path, const TextureLoadInfo& info) {
  OX_SCOPED_ZONE;

  Shared<TextureAsset> texture = create_shared<TextureAsset>(info);
  texture->asset_id = (uint32_t)asset_library.texture_assets.size() + 1;
  return asset_library.texture_assets.emplace(path, texture).first->second;
}

Shared<Mesh> AssetManager::load_mesh_asset(const std::string& path, uint32_t loadingFlags) {
  OX_SCOPED_ZONE;
  Shared<Mesh> asset = create_shared<Mesh>(path, loadingFlags);
  return asset_library.mesh_assets.emplace(path, asset).first->second;
}

Shared<AudioSource> AssetManager::load_audio_asset(const std::string& path) {
  OX_SCOPED_ZONE;
  Shared<AudioSource> source = create_shared<AudioSource>(path);
  return asset_library.audio_assets.emplace(path, source).first->second;
}

void AssetManager::free_unused_assets() {
  OX_SCOPED_ZONE;
  for (auto& [handle, asset] : asset_library.mesh_assets) {
    if (asset.use_count())
      return;
    asset_library.mesh_assets.erase(handle);
  }
  for (auto& [handle, asset] : asset_library.texture_assets) {
    if (asset.use_count())
      return;
    asset_library.texture_assets.erase(handle);
  }
}

void AssetManager::package_assets() {
  free_unused_assets();

  // TODO(hatrickek): Pack materials inside of meshes(glb).
  // TODO(hatrickek): Pack/compress audio files.

  constexpr auto meshDirectory = "Assets/Objects";
  constexpr auto textureDirectory = "Assets/Textures";
  //constexpr auto soundsDirectory = "Assets/Sounds";
  //constexpr auto scenesDirectory = "Assets/Scenes";

  std::filesystem::create_directory("Assets");

  // Package mesh files
  for (auto& [path, asset] : asset_library.mesh_assets) {
    const auto filePath = std::filesystem::path(path);
    auto outPath = std::filesystem::path(meshDirectory) / filePath.filename();
    outPath = outPath.generic_string();
    auto dir = meshDirectory;
    std::filesystem::create_directory(dir);
    const auto exported = Mesh::export_as_binary(path, outPath.string());
    if (!exported)
      OX_CORE_ERROR("Couldn't export mesh asset: {}", path);
    else
      OX_CORE_INFO("Exported mesh asset to: {}", outPath.string());
  }

  // Package texture files
  for (auto& [path, asset] : asset_library.texture_assets) {
    const auto filePath = std::filesystem::path(path);
    const auto outPath = std::filesystem::path(textureDirectory) / filePath.filename();
    std::filesystem::create_directory(textureDirectory);
    const auto exported = copy_file(path, outPath);
    if (!exported)
      OX_CORE_ERROR("Couldn't export image asset: {}", path);
    else
      OX_CORE_INFO("Exported image asset to: {}", outPath.string());
  }
}
}
