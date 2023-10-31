#pragma once
#include <nfd.h>

#include <string>
#include <vector>

namespace Oxylus {
class FileDialogs {
public:
  static void init_nfd();

  static void close_nfd();

  /// Returns empty strings if cancelled
  static std::string open_file(const std::vector<nfdfilteritem_t>& filter);

  /// Returns empty strings if cancelled
  static std::string save_file(const std::vector<nfdfilteritem_t>& filter, const char* defaultName);

  static void open_folder_and_select_item(const char* path);
  static void open_file_with_program(const char* filepath);
};
}
