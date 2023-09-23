#include "AssetManager.h"

#include "MaterialSerializer.h"
#include "Core/Project.h"
#include "Render/Mesh.h"

#include "Utils/Log.h"
#include "Utils/Profiler.h"

namespace Oxylus {
AssetManager::AssetLibrary AssetManager::s_Library;
std::mutex AssetManager::s_AssetMutex;

std::filesystem::path AssetManager::GetAssetFileSystemPath(const std::filesystem::path& path) {
  return Project::GetAssetDirectory() / path;
}

Ref<TextureAsset> AssetManager::GetTextureAsset(const TextureLoadInfo& info) {
  if (s_Library.m_TextureAssets.contains(info.Path)) {
    return s_Library.m_TextureAssets[info.Path];
  }

  return LoadTextureAsset(info.Path, info);
}

Ref<TextureAsset> AssetManager::GetTextureAsset(const std::string& name, const TextureLoadInfo& info) {
  if (s_Library.m_TextureAssets.contains(name)) {
    return s_Library.m_TextureAssets[name];
  }

  return LoadTextureAsset(name, info);
}

Ref<Mesh> AssetManager::GetMeshAsset(const std::string& path, const int32_t loadingFlags) {
  OX_SCOPED_ZONE;
  if (s_Library.m_MeshAssets.contains(path)) {
    return s_Library.m_MeshAssets[path];
  }

  return LoadMeshAsset(path, loadingFlags);
}

Ref<TextureAsset> AssetManager::LoadTextureAsset(const std::string& path) {
  OX_SCOPED_ZONE;

  std::lock_guard lock(s_AssetMutex);

  Ref<TextureAsset> texture = CreateRef<TextureAsset>(path);
  return s_Library.m_TextureAssets.emplace(path, texture).first->second;
}

Ref<TextureAsset> AssetManager::LoadTextureAsset(const std::string& path, const TextureLoadInfo& info) {
  OX_SCOPED_ZONE;

  std::lock_guard lock(s_AssetMutex);

  Ref<TextureAsset> texture = CreateRef<TextureAsset>(info);
  return s_Library.m_TextureAssets.emplace(path, texture).first->second;
}

Ref<Mesh> AssetManager::LoadMeshAsset(const std::string& path, int32_t loadingFlags) {
  OX_SCOPED_ZONE;
  Ref<Mesh> asset = CreateRef<Mesh>(path, loadingFlags);
  return s_Library.m_MeshAssets.emplace(path, asset).first->second;
}

void AssetManager::FreeUnusedAssets() {
  OX_SCOPED_ZONE;
  for (auto& [handle, asset] : s_Library.m_MeshAssets) {
    if (asset.use_count())
      return;
    s_Library.m_MeshAssets.erase(handle);
  }
  for (auto& [handle, asset] : s_Library.m_TextureAssets) {
    if (asset.use_count())
      return;
    s_Library.m_TextureAssets.erase(handle);
  }
}

void AssetManager::PackageAssets() {
  FreeUnusedAssets();

  // TODO(hatrickek): Pack materials inside of meshes(glb).
  // TODO(hatrickek): Pack/compress audio files.

  constexpr auto meshDirectory = "Assets/Objects";
  constexpr auto textureDirectory = "Assets/Textures";
  //constexpr auto soundsDirectory = "Assets/Sounds";
  //constexpr auto scenesDirectory = "Assets/Scenes";

  std::filesystem::create_directory("Assets");

  // Package mesh files
  for (auto& [path, asset] : s_Library.m_MeshAssets) {
    const auto filePath = std::filesystem::path(path);
    auto outPath = std::filesystem::path(meshDirectory) / filePath.filename();
    outPath = outPath.generic_string();
    auto dir = meshDirectory;
    std::filesystem::create_directory(dir);
    const auto exported = Mesh::ExportAsBinary(path, outPath.string());
    if (!exported)
      OX_CORE_ERROR("Couldn't export mesh asset: {}", path);
    else
      OX_CORE_INFO("Exported mesh asset to: {}", outPath.string());
  }

  // Package texture files
  for (auto& [path, asset] : s_Library.m_TextureAssets) {
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
