#include "LuaComponentBindings.h"

#include <sol/state.hpp>

#include "Scene/Entity.h"
#include "Scripting/Sol2Helpers.h"

namespace Ox {
void LuaBindings::bind_tag_component(const Shared<sol::state>& state) {
  auto name_component_type = state->new_usertype<TagComponent>("NameComponent");
  SET_TYPE_FIELD(name_component_type, TagComponent, tag);
  REGISTER_COMPONENT_WITH_ECS(state, TagComponent)
}

void LuaBindings::bind_transform_component(const Shared<sol::state>& state) {
  auto transform_component_type = state->new_usertype<TransformComponent>("TransformComponent");
  SET_TYPE_FIELD(transform_component_type, TransformComponent, position);
  SET_TYPE_FIELD(transform_component_type, TransformComponent, rotation);
  SET_TYPE_FIELD(transform_component_type, TransformComponent, scale);
  REGISTER_COMPONENT_WITH_ECS(state, TransformComponent)
}

void LuaBindings::bind_light_component(const Shared<sol::state>& state) {
  auto light_component_type = state->new_usertype<LightComponent>("LightComponent");
  SET_TYPE_FIELD(light_component_type, LightComponent, color);
  SET_TYPE_FIELD(light_component_type, LightComponent, intensity);
  REGISTER_COMPONENT_WITH_ECS(state, LightComponent)
}
}
