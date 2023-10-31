#include "UIUtils.h"
#include "Render/Window.h"

#include "Core/PlatformDetection.h"

#ifdef OX_PLATFORM_WINDOWS
#include <ShlObj_core.h>
#include <comdef.h>
#include <shellapi.h>
#endif

namespace Oxylus {
void FileDialogs::init_nfd() {
  NFD_Init();
}

void FileDialogs::close_nfd() {
  NFD_Quit();
}

void FileDialogs::open_file_with_program(const char* filepath) {
#ifdef OX_PLATFORM_WINDOWS
  const _bstr_t widePath(filepath);
  ShellExecute(nullptr, LPCSTR(L"open"), widePath, nullptr, nullptr, SW_RESTORE);
#else
    OX_CORE_WARN("Not implemented on this platform!");
#endif
}

std::string FileDialogs::open_file(const std::vector<nfdfilteritem_t>& filter) {
  nfdchar_t* outPath;
  const nfdresult_t result = NFD_OpenDialog(&outPath, filter.data(), (uint32_t)filter.size(), nullptr);
  if (result == NFD_OKAY) {
    return outPath;
  }
  return {};
}

std::string FileDialogs::save_file(const std::vector<nfdfilteritem_t>& filter, const char* defaultName) {
  nfdchar_t* outPath;
  const nfdresult_t result = NFD_SaveDialog(&outPath, filter.data(), (uint32_t)filter.size(), nullptr, defaultName);
  if (result == NFD_OKAY) {
    return outPath;
  }
  return {};
}

void FileDialogs::open_folder_and_select_item(const char* path) {
#ifdef OX_PLATFORM_WINDOWS
  const _bstr_t widePath(path);
  if (const auto pidl = ILCreateFromPath(widePath)) {
    SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
    ILFree(pidl);
  }
#else
    OX_CORE_WARN("Not implemented on this platform!");
#endif
}
}
