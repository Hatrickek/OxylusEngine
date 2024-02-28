#include "LuaComponentBindings.h"

#include <sol/state.hpp>

#include "Assets/AssetManager.h"

#include "Scene/Entity.h"
#include "LuaHelpers.h"

#include "Scene/Components.h"

namespace Ox {
void LuaBindings::bind_components(const Shared<sol::state>& state) {
  bind_tag_component(state);
  bind_transform_component(state);
  bind_light_component(state);
  bind_mesh_component(state);
  bind_camera_component(state);
}

void LuaBindings::bind_tag_component(const Shared<sol::state>& state) {
  auto name_component_type = state->new_usertype<TagComponent>("TagComponent");
  SET_COMPONENT_TYPE_ID(name_component_type, TagComponent);
  SET_TYPE_FIELD(name_component_type, TagComponent, tag);
  SET_TYPE_FIELD(name_component_type, TagComponent, enabled);
  register_meta_component<TagComponent>();
}

void LuaBindings::bind_transform_component(const Shared<sol::state>& state) {
  auto transform_component_type = state->new_usertype<TransformComponent>("TransformComponent");
  SET_COMPONENT_TYPE_ID(transform_component_type, TransformComponent);
  SET_TYPE_FIELD(transform_component_type, TransformComponent, position);
  SET_TYPE_FIELD(transform_component_type, TransformComponent, rotation);
  SET_TYPE_FIELD(transform_component_type, TransformComponent, scale);
  register_meta_component<TransformComponent>();
}

void LuaBindings::bind_light_component(const Shared<sol::state>& state) {
  // TODO:
  auto light_component_type = state->new_usertype<LightComponent>("LightComponent");
  SET_COMPONENT_TYPE_ID(light_component_type, LightComponent);
  SET_TYPE_FIELD(light_component_type, LightComponent, color);
  SET_TYPE_FIELD(light_component_type, LightComponent, intensity);
  register_meta_component<LightComponent>();
}

void LuaBindings::bind_mesh_component(const Shared<sol::state>& state) {
  auto material = state->new_usertype<Material>("Material");
  SET_TYPE_FUNCTION(material, Material, get_name);
  SET_TYPE_FUNCTION(material, Material, set_color);
  SET_TYPE_FUNCTION(material, Material, set_emissive);
  SET_TYPE_FUNCTION(material, Material, set_roughness);
  SET_TYPE_FUNCTION(material, Material, set_metallic);
  SET_TYPE_FUNCTION(material, Material, set_reflectance);
  const std::initializer_list<std::pair<sol::string_view, Material::AlphaMode>> alpha_mode = {
    {"Opaque", Material::AlphaMode::Opaque},
    {"Blend", Material::AlphaMode::Blend},
    {"Mask", Material::AlphaMode::Mask}
  };
  state->new_enum<Material::AlphaMode, true>("AlphaMode", alpha_mode);
  SET_TYPE_FUNCTION(material, Material, set_alpha_mode);
  SET_TYPE_FUNCTION(material, Material, set_alpha_cutoff);
  SET_TYPE_FUNCTION(material, Material, set_double_sided);
  SET_TYPE_FUNCTION(material, Material, is_opaque);
  SET_TYPE_FUNCTION(material, Material, alpha_mode_to_string);

  material.set_function("new", [](const std::string& name) -> Shared<Material> { return create_shared<Material>(name); });

  auto mesh_component_type = state->new_usertype<MeshComponent>("MeshComponent");
  SET_COMPONENT_TYPE_ID(mesh_component_type, MeshComponent);
  SET_TYPE_FIELD(mesh_component_type, MeshComponent, mesh_base);
  SET_TYPE_FIELD(mesh_component_type, MeshComponent, node_index);
  SET_TYPE_FIELD(mesh_component_type, MeshComponent, cast_shadows);
  SET_TYPE_FIELD(mesh_component_type, MeshComponent, materials);
  SET_TYPE_FIELD(mesh_component_type, MeshComponent, aabb);
  register_meta_component<MeshComponent>();
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
  SET_TYPE_FIELD(camera_type, Camera, get_forward);
  SET_TYPE_FIELD(camera_type, Camera, get_right);
  SET_TYPE_FIELD(camera_type, Camera, get_screen_ray);

  auto camera_component_type = state->new_usertype<CameraComponent>("CameraComponent");
  SET_COMPONENT_TYPE_ID(camera_component_type, CameraComponent);
  SET_TYPE_FIELD(camera_component_type, CameraComponent, camera);
  register_meta_component<CameraComponent>();
}
}
