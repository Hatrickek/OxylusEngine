#include "ModuleManager.h"

#include "Utils/Log.h"

namespace Ox {
dylib* ModuleManager::add_lib(std::string_view name, std::string_view path) {
  try {
    libs.emplace(name, create_unique<dylib>(path));
    return libs[name].get();
  }
  catch (const std::exception& exc) {
    OX_CORE_ERROR("{}", exc.what());
    return nullptr;
  }
}

void ModuleManager::clear() {
  libs.clear();
}

ModuleManager* ModuleManager::get() {
  static ModuleManager manager{};
  return &manager;
}
}
