#pragma once
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace ox {
/// @brief A lightweight std::filesystem alternative
class FileSystem {
public:
  /// @return directory and file
  static std::pair<std::string, std::string> split_path(std::string_view full_path);
  /// @return What's left after the last dot in `filepath`
  static std::string get_file_extension(std::string_view filepath);
  /// @return File name without the extension
  static std::string get_file_name(std::string_view filepath);
  /// @return File name with the extension
  static std::string get_name_with_extension(std::string_view filepath);
  /// @return Directory from the given path
  static std::string get_directory(std::string_view path);
  /// @brief append a pair of paths together
  static std::string append_paths(std::string_view path, std::string_view second_path);
  /// @brief convert paths with '\\' into '/'
  static std::string preferred_path(std::string_view path);
  /// @brief open and select the file in OS
  static void open_folder_select_file(std::string_view path);
  /// @brief open the file in an external program
  static void open_file_externally(std::string_view path);

  static std::string read_file(std::string_view file_path);
  static std::vector<uint8_t> read_file_binary(std::string_view file_path);
  static std::string read_shader_file(const std::string& shader_file_name);
  static std::string get_shader_path(const std::string& shader_file_name);

  template <typename T> static bool write_file(const std::string_view file_path, const T& data, const std::string& comment = {}) {
    std::stringstream ss;
    ss << comment << "\n" << data;
    std::ofstream filestream(file_path.data());
    if (filestream.is_open()) {
      filestream << ss.str();
      return true;
    }

    return false;
  }

  static bool write_file_binary(std::string_view file_path, const std::vector<uint8_t>& data);

  static bool binary_to_header(std::string_view file_path, std::string_view data_name, const std::vector<uint8_t>& data);
};
} // namespace ox
