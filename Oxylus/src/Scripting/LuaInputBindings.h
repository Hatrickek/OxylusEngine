#pragma once

#include "Core/Base.h"

namespace sol {
class state;
}

namespace Oxylus::LuaBindings {
void bind_lua_input(const Shared<sol::state>& state);  
}
