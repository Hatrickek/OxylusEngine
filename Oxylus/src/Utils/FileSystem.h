#pragma once
#include <string>

namespace Oxylus {
/**
 * \brief A lightweight std::filesystem alternative 
 */
class FileSystem {
public:
  // Returns what left after the last dot in `filepath`
  static std::string GetFileExtension(std::string_view filepath);
  static std::string GetFileName(std::string_view filepath);
  static std::string GetNameWithExtension(std::string_view filepath);
};
}
