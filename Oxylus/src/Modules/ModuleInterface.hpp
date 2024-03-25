#pragma once

#define OX_BUILD_DLL
#include "Linker.hpp"

#include <entt/locator/locator.hpp>
#include <entt/meta/context.hpp>
#include <sol/state.hpp>

namespace ox {
class OX_SHARED ModuleInterface {
public:
  virtual ~ModuleInterface() = default;

  virtual void register_components(sol::state* state, const entt::locator<entt::meta_ctx>::node_type& ctx) = 0;
  virtual void unregister_components(sol::state* state, const entt::locator<entt::meta_ctx>::node_type& ctx) = 0;
};

// use this function return a heap allocated ModuleInterface
#define CREATE_MODULE_FUNC extern "C" OX_SHARED ModuleInterface* create_module()
}
