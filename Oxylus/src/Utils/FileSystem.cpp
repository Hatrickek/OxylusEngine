#include "FileSystem.h"

#include "StringUtils.h"

#include "Core/PlatformDetection.h"

namespace Ox {
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
  std::string new_path(path);
  new_path.append("/").append(second_path);
  return new_path;
}

std::string FileSystem::preferred_path(const std::string_view path) {
  std::string new_path(path);
#ifdef OX_PLATFORM_WINDOWS
  StringUtils::replace_string(new_path, "/", "\\");
#else
  StringUtils::replace_string(new_path, "\\", "/");
#endif
  return new_path;
}
}
