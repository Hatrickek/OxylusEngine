#pragma once
#include <string>

namespace Oxylus {
/**
 * \brief A lightweight std::filesystem alternative 
 */
class FileSystem {
public:
  // Returns what left after the last dot in `filepath`
  static std::string get_file_extension(std::string_view filepath);
  static std::string get_file_name(std::string_view filepath);
  static std::string get_name_with_extension(std::string_view filepath);
};
}
