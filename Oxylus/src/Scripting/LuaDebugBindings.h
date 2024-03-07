#pragma once
#include "Core/Base.h"

namespace sol {
class state;
}

namespace ox::LuaBindings {
void bind_debug_renderer(const Shared<sol::state>& state);
}
