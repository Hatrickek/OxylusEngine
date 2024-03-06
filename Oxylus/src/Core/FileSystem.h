#pragma once
#include <string>

namespace Ox {
/// @brief A lightweight std::filesystem alternative 
class FileSystem {
public:
  /// Returns what left after the last dot in `filepath`
  static std::string get_file_extension(std::string_view filepath);
  static std::string get_file_name(std::string_view filepath);
  static std::string get_name_with_extension(std::string_view filepath);
  /// append a pair of paths together
  static std::string append_paths(std::string_view path, std::string_view second_path);
  /// convert paths with '\\' into '/'
  static std::string preferred_path(std::string_view path);
  /// open and select the file in OS
  static void open_folder_select_file(std::string_view path);
  /// open the file in an external program
  static void open_file_externally(std::string_view path);
};
}
