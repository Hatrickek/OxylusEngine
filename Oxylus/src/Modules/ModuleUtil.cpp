#include "ModuleUtil.hpp"

#include "ModuleInterface.hpp"
#include "ModuleRegistry.hpp"

#include "Core/App.hpp"

#include "Scripting/LuaManager.hpp"

#include "Utils/Log.hpp"

namespace ox {
void ModuleUtil::load_module(const std::string& name, const std::string& path) {
  const auto lib = App::get_system<ModuleRegistry>()->add_lib(name, path);
  if (!lib)
    return;

  auto* state = App::get_system<LuaManager>()->get_state();
  lib->interface->register_components(state, entt::locator<entt::meta_ctx>::handle());
  OX_LOG_INFO("Successfully loaded module: {}", name);
}

void ModuleUtil::unload_module(const std::string& module_name) {
  const auto lib = App::get_system<ModuleRegistry>()->get_lib(module_name);
  if (!lib)
    return;

  auto* state = App::get_system<LuaManager>()->get_state();
  lib->interface->unregister_components(state, entt::locator<entt::meta_ctx>::handle());
}
}
