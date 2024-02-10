#include "LuaUIBindings.h"

#include <sol/state.hpp>

#include "LuaImGuiBindings.h"
#include "Sol2Helpers.h"

#include "UI/OxUI.h"

namespace Ox::LuaBindings {
void bind_ui(const Shared<sol::state>& state) {
  LuaImGuiBindings::init(state.get());

  auto uitbl = state->create_table("UI");
  uitbl.set_function("begin_properties",[] {return OxUI::begin_properties(OxUI::default_properties_flags);});
  uitbl.set_function("begin_properties_flags", [](const ImGuiTableFlags flags) { return OxUI::begin_properties(flags); });
  uitbl.set_function("end_properties", &OxUI::end_properties);
  uitbl.set_function("text", [](const char* text1, const char* text2) { OxUI::text(text1, text2); });
  uitbl.set_function("text_tooltip", &OxUI::text);
  uitbl.set_function("property_bool", [](const char* label, bool* flag) { OxUI::property(label, flag); });
  uitbl.set_function("property_bool_tooltip", [](const char* label, bool* flag, const char* tooltip) { OxUI::property(label, flag, tooltip); });
  uitbl.set_function("property_input_field", [](const char* label, std::string* text, ImGuiInputFlags flags) { OxUI::property(label, text, flags); });
  uitbl.set_function("property_input_field_tooltip", [](const char* label, std::string* text, ImGuiInputFlags flags, const char* tooltip) { OxUI::property(label, text, flags, tooltip); });

  // TODO: the rest of the api
}
}
