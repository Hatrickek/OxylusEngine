#pragma once
#include "Core/Base.h"

namespace sol {
class state;
}

namespace ox::LuaBindings {
void bind_audio(const Shared<sol::state>& state);
}
