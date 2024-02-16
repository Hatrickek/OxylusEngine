#pragma once

#include <dylib.hpp>
#include <string_view>

#include <ankerl/unordered_dense.h>

#include "Core/Base.h"

namespace Ox {
class ModuleInterface;

struct Module {
  Unique<dylib> lib;
  ModuleInterface* interface;
};

class ModuleRegistry {
public:
  Module* add_lib(std::string_view name, std::string_view path);
  Module* get_lib(std::string_view name);
  void clear();

  static ModuleRegistry* get();

private:
  ankerl::unordered_dense::map<std::string_view, Unique<Module>> libs = {};
};
}
