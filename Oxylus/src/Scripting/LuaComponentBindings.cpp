#include "LuaComponentBindings.h"

#include <sol/state.hpp>

#include "Assets/AssetManager.h"

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

void LuaBindings::bind_mesh_component(const Shared<sol::state>& state) {
  state->new_usertype<Mesh>("Mesh");

  auto mesh_component_type = state->new_usertype<MeshComponent>("MeshComponent");
  SET_TYPE_FIELD(mesh_component_type, MeshComponent, mesh_base);
  SET_TYPE_FIELD(mesh_component_type, MeshComponent, node_index);
  SET_TYPE_FIELD(mesh_component_type, MeshComponent, cast_shadows);
  REGISTER_COMPONENT_WITH_ECS(state, MeshComponent)
}
}
