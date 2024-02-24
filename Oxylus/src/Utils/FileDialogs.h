#pragma once
#include <nfd.h>

#include <string>
#include <vector>

#include "Core/ESystem.h"

namespace Ox {
class FileDialogs : public ESystem {
public:
  void init() override;
  void deinit() override;

  /// Returns empty strings if cancelled
  std::string open_file(const std::vector<nfdfilteritem_t>& filter);

  /// Returns empty strings if cancelled
  std::string save_file(const std::vector<nfdfilteritem_t>& filter, const char* defaultName);

  std::string open_dir();

  void open_folder_and_select_item(const char* path);
  void open_file_with_program(const char* filepath);
};
}