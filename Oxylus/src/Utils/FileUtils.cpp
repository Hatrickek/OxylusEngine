#include "FileUtils.h"

#include <filesystem>
#include <fstream>

#include "Log.h"
#include "Profiler.h"
#include "Core/Resources.h"

namespace Oxylus {
std::string FileUtils::read_file(const std::string& filePath) {
  std::ostringstream buf;
  const std::ifstream input(filePath.c_str());
  buf << input.rdbuf();
  return buf.str();
}

std::string FileUtils::read_shader_file(const std::string& shaderFileName) {
  auto value = read_file(get_shader_path(shaderFileName));
  OX_CORE_ASSERT(!value.empty())
  return value;
}

std::string FileUtils::get_shader_path(const std::string& shaderFileName) {
  const auto pathStr = Resources::get_resources_path("Shaders");
  const auto path = std::filesystem::path(pathStr) / shaderFileName;
  return path.string();
}
}
