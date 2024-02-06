#pragma once
#include "Core/Base.h"

namespace sol {
class state;
}

namespace Ox::LuaBindings {
void bind_renderer(const Shared<sol::state>& state);
}
