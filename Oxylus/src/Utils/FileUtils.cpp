#include "FileUtils.h"

#include <filesystem>
#include <fstream>

#include "Profiler.h"
#include "Core/Resources.h"
#include "Utils/Log.h"

namespace Oxylus {
std::optional<std::string> FileUtils::ReadFile(const std::string& filePath) {
  std::ostringstream buf;
  const std::ifstream input(filePath.c_str());
  OX_CORE_ASSERT(input)
  buf << input.rdbuf();
  return buf.str();
}

std::optional<std::string> FileUtils::ReadShaderFile(const std::string& shaderFileName) {
  const auto pathStr = Resources::GetResourcesPath("Shaders");
  const auto path = std::filesystem::path(pathStr) / shaderFileName;
  return ReadFile(path.string());
}

std::string FileUtils::GetShaderPath(const std::string& shaderFileName) {
  const auto pathStr = Resources::GetResourcesPath("Shaders");
  const auto path = std::filesystem::path(pathStr) / shaderFileName;
  return path.string();
}
}
