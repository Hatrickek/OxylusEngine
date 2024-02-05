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

void LuaBindings::bind_camera_component(const Shared<sol::state>& state) {
  auto camera_type = state->new_usertype<Camera>("Camera");
  SET_TYPE_FIELD(camera_type, Camera, set_yaw);
  SET_TYPE_FIELD(camera_type, Camera, set_pitch);
  SET_TYPE_FIELD(camera_type, Camera, get_yaw);
  SET_TYPE_FIELD(camera_type, Camera, get_pitch);
  SET_TYPE_FIELD(camera_type, Camera, set_near);
  SET_TYPE_FIELD(camera_type, Camera, set_far);
  SET_TYPE_FIELD(camera_type, Camera, get_near);
  SET_TYPE_FIELD(camera_type, Camera, get_far);
  SET_TYPE_FIELD(camera_type, Camera, get_fov);
  SET_TYPE_FIELD(camera_type, Camera, set_fov);
  SET_TYPE_FIELD(camera_type, Camera, get_aspect);
  SET_TYPE_FIELD(camera_type, Camera, get_front);
  SET_TYPE_FIELD(camera_type, Camera, get_right);

  auto camera_component_type = state->new_usertype<CameraComponent>("CameraComponent");
  SET_TYPE_FIELD(camera_component_type, CameraComponent, camera);
  REGISTER_COMPONENT_WITH_ECS(state, CameraComponent)
}
}
