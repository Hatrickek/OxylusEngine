#include "FileUtils.h"

#include <filesystem>
#include <fstream>

#include "Log.h"
#include "Profiler.h"

#include "Core/App.h"

namespace Ox {
std::string FileUtils::read_file(const std::string& file_path) {
  OX_SCOPED_ZONE;
  std::ostringstream buf;
  const std::ifstream input(file_path.c_str());
  buf << input.rdbuf();
  return buf.str();
}

std::string FileUtils::read_shader_file(const std::string& shader_file_name) {
  OX_SCOPED_ZONE;
  auto path = get_shader_path(shader_file_name);
  auto value = read_file(path);
  OX_CORE_ASSERT(!value.empty(), fmt::format("Shader file doesn't exist: {0}", shader_file_name.c_str()))
  return value;
}

std::string FileUtils::get_shader_path(const std::string& shader_file_name) {
  OX_SCOPED_ZONE;
  const auto path_str = std::filesystem::path(App::get()->get_specification().assets_path) / "Shaders";
  const auto path = std::filesystem::path(path_str) / shader_file_name;
  return path.string();
}
}
