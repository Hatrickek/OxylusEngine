#pragma once
#include "Core/Base.h"

namespace sol {
class state;
}

namespace Ox::LuaBindings {
void bind_components(const Shared<sol::state>& state);

void bind_tag_component(const Shared<sol::state>& state);
void bind_transform_component(const Shared<sol::state>& state);
void bind_light_component(const Shared<sol::state>& state);
void bind_mesh_component(const Shared<sol::state>& state);
void bind_camera_component(const Shared<sol::state>& state);
}
