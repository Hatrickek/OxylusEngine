#pragma once
#include "Core/Base.h"

namespace sol {
class state;
}

namespace Ox::LuaBindings {
void bind_asset_manager(const Shared<sol::state>& state);
}
