#pragma once
#include <nfd.h>

#include <string>
#include <vector>

namespace Oxylus {
  class FileDialogs {
  public:
    static void InitNFD();

    static void CloseNFD();

    /// Returns empty strings if cancelled
    static std::string OpenFile(const std::vector<nfdfilteritem_t>& filter);

    /// Returns empty strings if cancelled
    static std::string SaveFile(const std::vector<nfdfilteritem_t>& filter, const char* defaultName);

    static void OpenFolderAndSelectItem(const char* path);
    static void OpenFileWithProgram(const char* filepath);
  };
}
