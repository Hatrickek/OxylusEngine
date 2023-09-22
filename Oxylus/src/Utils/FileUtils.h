#pragma once

#include <optional>
#include <string>

namespace Oxylus {
class FileUtils {
public:
  static std::optional<std::string> ReadFile(const std::string& filePath);
  static std::optional<std::string> ReadShaderFile(const std::string& shaderFileName);
  static std::string GetShaderPath(const std::string& shaderFileName);
};
}
