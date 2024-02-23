#include "LuaApplicationBindings.h"

#include <sol/state.hpp>

#include "LuaHelpers.h"

#include "Core/App.h"

namespace Ox::LuaBindings {
void bind_application(const Shared<sol::state>& state) {
  auto app_table = state->create_table("Application");
  app_table.set_function("get_asset_directory", []() -> std::string { return App::get_asset_directory(); });
  app_table.set_function("get_asset_directory_absolute", []() -> std::string { return App::get_asset_directory_absolute(); });
}
}
