#include "LuaComponentBindings.h"

#include <sol/state.hpp>

#include "Assets/AssetManager.h"

#include "LuaHelpers.h"

#include "Scene/Components.h"

namespace ox {
void LuaBindings::bind_components(const Shared<sol::state>& state) {
  REGISTER_COMPONENT(state, TagComponent, FIELD(TagComponent, tag), FIELD(TagComponent, enabled));
#define TC TransformComponent
  REGISTER_COMPONENT(state, TC, FIELD(TC, position), FIELD(TC, rotation), FIELD(TC, scale));
  bind_mesh_component(state);
  bind_camera_component(state);
}

void LuaBindings::bind_light_component(const Shared<sol::state>& state) {
  REGISTER_COMPONENT(state, LightComponent, FIELD(LightComponent, color), FIELD(LightComponent, intensity)); // TODO: Rest
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

#define MC MeshComponent
  REGISTER_COMPONENT(state, MC, FIELD(MC, mesh_base), FIELD(MC, node_index), FIELD(MC, cast_shadows), FIELD(MC, materials), FIELD(MC, aabb));
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

  REGISTER_COMPONENT(state, CameraComponent, FIELD(CameraComponent, camera));
}
}
