#include "FileUtils.h"

#include <filesystem>
#include <fstream>

#include "Profiler.h"
#include "Core/Resources.h"
#include "Utils/Log.h"

namespace Oxylus {
std::optional<std::string> FileUtils::read_file(const std::string& filePath) {
  std::ostringstream buf;
  const std::ifstream input(filePath.c_str());
  OX_CORE_ASSERT(input)
  buf << input.rdbuf();
  return buf.str();
}

std::optional<std::string> FileUtils::read_shader_file(const std::string& shaderFileName) {
  const auto pathStr = Resources::get_resources_path("Shaders");
  const auto path = std::filesystem::path(pathStr) / shaderFileName;
  return read_file(path.string());
}

std::string FileUtils::get_shader_path(const std::string& shaderFileName) {
  const auto pathStr = Resources::get_resources_path("Shaders");
  const auto path = std::filesystem::path(pathStr) / shaderFileName;
  return path.string();
}
}
