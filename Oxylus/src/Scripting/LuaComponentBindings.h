#pragma once
#include "Core/Base.h"

namespace sol {
class state;
}

namespace Oxylus::LuaBindings {
void bind_tag_component(const Shared<sol::state>& state);
void bind_transform_component(const Shared<sol::state>& state);
void bind_light_component(const Shared<sol::state>& state);
}
