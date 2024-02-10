#include "LuaApplicationBindings.h"

#include <sol/state.hpp>

#include "Sol2Helpers.h"

#include "Core/Application.h"

namespace Ox::LuaBindings {
void bind_application(const Shared<sol::state>& state) {
  auto app_table = state->create_table("Application");
  SET_TYPE_FUNCTION(app_table, Application, get_asset_directory);
  SET_TYPE_FUNCTION(app_table, Application, get_asset_directory_absolute);
}
}
