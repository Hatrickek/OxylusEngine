#pragma once

#include <string>

namespace Ox {
class FileUtils {
public:
  static std::string read_file(const std::string& file_path);
  static std::string read_shader_file(const std::string& shader_file_name);
  static std::string get_shader_path(const std::string& shader_file_name);
};
}
