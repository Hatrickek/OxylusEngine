#include "AssetManager.h"

#include "MaterialSerializer.h"
#include "Core/Project.h"
#include "Render/Mesh.h"

#include "Utils/Log.h"
#include "Utils/Profiler.h"

namespace Oxylus {
AssetManager::AssetLibrary AssetManager::s_library;
std::mutex AssetManager::s_asset_mutex;

std::filesystem::path AssetManager::get_asset_file_system_path(const std::filesystem::path& path) {
  return Project::get_asset_directory() / path;
}

Ref<TextureAsset> AssetManager::get_texture_asset(const TextureLoadInfo& info) {
  if (s_library.texture_assets.contains(info.path)) {
    return s_library.texture_assets[info.path];
  }

  return load_texture_asset(info.path, info);
}

Ref<TextureAsset> AssetManager::get_texture_asset(const std::string& name, const TextureLoadInfo& info) {
  if (s_library.texture_assets.contains(name)) {
    return s_library.texture_assets[name];
  }

  return load_texture_asset(name, info);
}

Ref<Mesh> AssetManager::get_mesh_asset(const std::string& path, const int32_t loadingFlags) {
  OX_SCOPED_ZONE;
  if (s_library.mesh_assets.contains(path)) {
    return s_library.mesh_assets[path];
  }

  return load_mesh_asset(path, loadingFlags);
}

Ref<TextureAsset> AssetManager::load_texture_asset(const std::string& path) {
  OX_SCOPED_ZONE;

  std::lock_guard lock(s_asset_mutex);

  Ref<TextureAsset> texture = create_ref<TextureAsset>(path);
  return s_library.texture_assets.emplace(path, texture).first->second;
}

Ref<TextureAsset> AssetManager::load_texture_asset(const std::string& path, const TextureLoadInfo& info) {
  OX_SCOPED_ZONE;

  std::lock_guard lock(s_asset_mutex);

  Ref<TextureAsset> texture = create_ref<TextureAsset>(info);
  return s_library.texture_assets.emplace(path, texture).first->second;
}

Ref<Mesh> AssetManager::load_mesh_asset(const std::string& path, int32_t loadingFlags) {
  OX_SCOPED_ZONE;
  Ref<Mesh> asset = create_ref<Mesh>(path, loadingFlags);
  return s_library.mesh_assets.emplace(path, asset).first->second;
}

void AssetManager::free_unused_assets() {
  OX_SCOPED_ZONE;
  for (auto& [handle, asset] : s_library.mesh_assets) {
    if (asset.use_count())
      return;
    s_library.mesh_assets.erase(handle);
  }
  for (auto& [handle, asset] : s_library.texture_assets) {
    if (asset.use_count())
      return;
    s_library.texture_assets.erase(handle);
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
  for (auto& [path, asset] : s_library.mesh_assets) {
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
  for (auto& [path, asset] : s_library.texture_assets) {
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
