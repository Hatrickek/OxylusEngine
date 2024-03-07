#include "FileSystem.h"

#include "Core/PlatformDetection.h"

#include "Utils/StringUtils.h"

#ifdef OX_PLATFORM_WINDOWS
#include <comdef.h>
#include <shellapi.h>
#include <ShlObj_core.h>
#endif

#include "Utils/Log.h"

namespace ox {
std::string FileSystem::get_file_extension(const std::string_view filepath) {
  const auto lastDot = filepath.find_last_of('.');
  return static_cast<std::string>(filepath.substr(lastDot + 1, filepath.size() - lastDot));
}

std::string FileSystem::get_file_name(const std::string_view filepath) {
  auto lastSlash = filepath.find_last_of("/\\");
  lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;
  const auto lastDot = filepath.rfind('.');
  const auto count = lastDot == std::string::npos ? filepath.size() - lastSlash : lastDot - lastSlash;
  return static_cast<std::string>(filepath.substr(lastSlash, count));
}

std::string FileSystem::get_name_with_extension(const std::string_view filepath) {
  auto lastSlash = filepath.find_last_of("/\\");
  lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;
  return static_cast<std::string>(filepath.substr(lastSlash, filepath.size()));
}

std::string FileSystem::append_paths(const std::string_view path, const std::string_view second_path) {
  if (path.empty() || second_path.empty())
    return {};

  std::string new_path(path);
  new_path.append("/").append(second_path);
  return preferred_path(new_path);
}

std::string FileSystem::preferred_path(const std::string_view path) {
  std::string new_path(path);
  StringUtils::replace_string(new_path, "\\", "/");
  return new_path;
}

void FileSystem::open_folder_select_file(std::string_view path) {
#ifdef OX_PLATFORM_WINDOWS
  std::string p_str(path);
  StringUtils::replace_string(p_str, "/", "\\");
  const _bstr_t widePath(p_str.c_str());
  if (const auto pidl = ILCreateFromPath(widePath)) {
    SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0); // TODO: check for result
    ILFree(pidl);
  }
#else
    OX_CORE_WARN("Not implemented on this platform!");
#endif
}

void FileSystem::open_file_externally(std::string_view path) {
#ifdef OX_PLATFORM_WINDOWS
  const _bstr_t widePath(path.data());
  ShellExecute(nullptr, LPCSTR(L"open"), widePath, nullptr, nullptr, SW_RESTORE);
#else
    OX_CORE_WARN("Not implemented on this platform!");
#endif
}
}
