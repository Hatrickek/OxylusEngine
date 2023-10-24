#pragma once

#include <optional>
#include <string>

namespace Oxylus {
class FileUtils {
public:
  static std::string read_file(const std::string& filePath);
  static std::string read_shader_file(const std::string& shaderFileName);
  static std::string get_shader_path(const std::string& shaderFileName);
};
}
