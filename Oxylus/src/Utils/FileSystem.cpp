﻿#include "FileSystem.h"


namespace Oxylus {
std::string FileSystem::GetFileExtension(std::string_view filepath) {
  const auto lastDot = filepath.find_last_of('.');
  return static_cast<std::string>(filepath.substr(lastDot + 1, filepath.size() - lastDot));
}

std::string FileSystem::GetFileName(std::string_view filepath) {
  auto lastSlash = filepath.find_last_of("/\\");
  lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;
  const auto lastDot = filepath.rfind('.');
  const auto count = lastDot == std::string::npos ? filepath.size() - lastSlash : lastDot - lastSlash;
  return static_cast<std::string>(filepath.substr(lastSlash, count));
}

std::string FileSystem::GetNameWithExtension(std::string_view filepath) {
  auto lastSlash = filepath.find_last_of("/\\");
  lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;
  return static_cast<std::string>(filepath.substr(lastSlash, filepath.size()));
}
}