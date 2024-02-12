#include "LuaApplicationBindings.h"

#include <sol/state.hpp>

#include "Sol2Helpers.h"

#include "Core/Application.h"

namespace Ox::LuaBindings {
void bind_application(const Shared<sol::state>& state) {
  auto app_table = state->create_table("Application");
  app_table.set_function("get_asset_directory", []() -> std::string { return Application::get_asset_directory(); });
  app_table.set_function("get_asset_directory_absolute", []() -> std::string { return Application::get_asset_directory_absolute(); });
}
}
