#include "src/oxpch.h"
#include "UIUtils.h"
#include "Render/Window.h"

#ifdef OX_PLATFORM_WINDOWS

#include <ShlObj_core.h>
#include <comdef.h>

#endif

namespace Oxylus {

  void FileDialogs::InitNFD() {
    NFD_Init();
  }

  void FileDialogs::CloseNFD() {
    NFD_Quit();
  }

  std::string FileDialogs::OpenFile(const std::vector<nfdfilteritem_t>& filter) {
    nfdchar_t* outPath;
    nfdresult_t result = NFD_OpenDialog(&outPath, filter.data(), filter.size(), nullptr);
    if (result == NFD_OKAY) {
      return outPath;
    }
    return {};
  }

  std::string FileDialogs::SaveFile(const std::vector<nfdfilteritem_t>& filter, const char* defaultName) {
    nfdchar_t* outPath;
    nfdresult_t result = NFD_SaveDialog(&outPath, filter.data(), filter.size(), nullptr, defaultName);
    if (result == NFD_OKAY) {
      return outPath;
    }
    return {};
  }

  void FileDialogs::OpenFolderAndSelectItem(const char* path) {
#ifdef OX_PLATFORM_WINDOWS
    const _bstr_t widePath(path);
    if (const LPITEMIDLIST pidl = ILCreateFromPath(widePath)) {
      SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
      ILFree(pidl);
    }
#else
    OX_CORE_ERROR("Not implemented on this platform!");
#endif
  }

}
