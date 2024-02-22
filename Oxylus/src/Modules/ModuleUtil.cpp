#include "ModuleUtil.h"

#include "ModuleInterface.h"
#include "ModuleRegistry.h"

#include "Scripting/LuaManager.h"

#include "Utils/Log.h"

namespace Ox {
void ModuleUtil::load_module(const std::string& name, const std::string& path) {
  const auto lib = ModuleRegistry::get()->add_lib(name, path);
  if (!lib)
    return;

  auto* state = LuaManager::get()->get_state();
  lib->interface->register_components(state, entt::locator<entt::meta_ctx>::handle());
  OX_CORE_INFO("Successfully loaded module: {}", name);
}

void ModuleUtil::unload_module(const std::string& module_name) {
  const auto lib = ModuleRegistry::get()->get_lib(module_name);
  if (!lib)
    return;

  auto* state = LuaManager::get()->get_state();
  lib->interface->unregister_components(state, entt::locator<entt::meta_ctx>::handle());
}
}
