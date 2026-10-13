#pragma once

#include <RmlUi/Core/FileInterface.h>
#include <filesystem>
#include <string_view>

#include "Core/Types.hpp"

namespace ox {
class AssetManager;
class VFS;
enum class AssetType : u32;

// Where a path RmlUi hands over lives on disk. One that starts with a VFS mount (`assets_dir/UI/hud.rml`) resolves
// through it, so a document is named the same in the editor and in a shipped game, and the paths RmlUi joins against
// it (a linked `style.rcss`) stay virtual too. Anything else is the path it is, relative to the working directory as
// RmlUi's own interface reads it.
auto resolve_rml_path(const VFS* vfs, std::string_view path) -> std::filesystem::path;

// The pack an imported asset of `type` was cooked into, for the source `path` names. Empty when it names no such asset
// or the asset isn't cooked, and then there is only the file.
auto find_rml_pack(AssetManager& asset_man, const VFS* vfs, std::string_view path, AssetType type)
  -> std::filesystem::path;

// Every file RmlUi reads goes through here: documents, stylesheets, templates, font faces. The source file while it is
// there, which is what a pack was cooked from and newer than it while someone edits it, and the cooked pack once it
// isn't: a shipped game has only the pack. A file is read whole on open, they are small and RmlUi reads most of them
// in one go anyway.
struct RmlFileInterface : Rml::FileInterface {
  // the mounts paths resolve against, the app's once `RmlUI` is up, none and every path physical without one
  const VFS* vfs = nullptr;
  // where a cooked document is found once its source is gone, none and only files are read
  AssetManager* asset_man = nullptr;

  auto Open(const Rml::String& path) -> Rml::FileHandle override;
  auto Close(Rml::FileHandle file) -> void override;
  auto Read(void* buffer, usize size, Rml::FileHandle file) -> usize override;
  auto Seek(Rml::FileHandle file, long offset, i32 origin) -> bool override;
  auto Tell(Rml::FileHandle file) -> usize override;
  auto Length(Rml::FileHandle file) -> usize override;
  auto LoadFile(const Rml::String& path, Rml::String& out_data) -> bool override;
};
} // namespace ox
