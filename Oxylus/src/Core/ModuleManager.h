#pragma once

#include <dylib.hpp>
#include <string_view>

#include <ankerl/unordered_dense.h>

#include "Base.h"
#include "PlatformDetection.h"

#ifdef OX_PLATFORM_WINDOWS
#ifdef OX_BUILD_DLL
#define OX_API __declspec(dllexport)
#else
#define OX_API __declspec(dllimport)
#endif
#else
#define OX_API
#endif

namespace Ox {
class ModuleManager {
public:
  dylib* add_lib(std::string_view name, std::string_view path);
  void clear();

  static ModuleManager* get();

private:
  ankerl::unordered_dense::map<std::string_view, Unique<dylib>> libs = {};
};
}
