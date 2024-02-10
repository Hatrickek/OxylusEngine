#pragma once

#include "Core/Base.h"

namespace sol {
class state;
}

namespace Ox::LuaBindings {
void bind_ui(const Shared<sol::state>& state);
}
