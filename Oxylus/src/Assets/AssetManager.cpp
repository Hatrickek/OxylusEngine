#include "AssetManager.h"

#include <filesystem>

#include "Audio/AudioSource.h"
#include "Render/Mesh.h"

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
  texture->asset_id = (uint32_t)asset_library.texture_assets.size();
  return asset_library.texture_assets.emplace(path, texture).first->second;
}

Shared<TextureAsset> AssetManager::load_texture_asset(const std::string& path, const TextureLoadInfo& info) {
  OX_SCOPED_ZONE;

  Shared<TextureAsset> texture = create_shared<TextureAsset>(info);
  texture->asset_id = (uint32_t)asset_library.texture_assets.size();
  return asset_library.texture_assets.emplace(path, texture).first->second;
}

Shared<Mesh> AssetManager::load_mesh_asset(const std::string& path, uint32_t loadingFlags) {
  OX_SCOPED_ZONE;
  Shared<Mesh> asset = create_shared<Mesh>(path, loadingFlags);
  asset->asset_id = (uint32_t)asset_library.mesh_assets.size();
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
}
