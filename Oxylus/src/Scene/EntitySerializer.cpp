#include "EntitySerializer.h"

#include <fstream>

#include "SceneRenderer.h"

#include "Assets/AssetManager.h"

#include "Scene/Entity.h"

#include "Utils/FileUtils.h"

namespace Ox {
#define GET_STRING(node, name) node->as_table()->get(name)->as_string()->get()
#define GET_FLOAT(node, name) (float)node->as_table()->get(name)->as_floating_point()->get()
#define GET_UINT32(node, name) (uint32_t)node->as_table()->get(name)->as_integer()->get()
#define GET_BOOL(node, name) node->as_table()->get(name)->as_boolean()->get()
#define GET_ARRAY(node, name) node->as_table()->get(name)->as_array()

void EntitySerializer::serialize_entity(toml::array* entities, Entity entity) {
  entities->push_back(toml::table{{"uuid", std::to_string((uint64_t)entity.get_uuid())}});

  if (entity.has_component<TagComponent>()) {
    const auto& tag = entity.get_component<TagComponent>();

    const auto table = toml::table{
      {"tag", tag.tag},
      {"enabled", tag.enabled}
    };

    entities->push_back(toml::table{{"tag_component", table}});
  }

  if (entity.has_component<RelationshipComponent>()) {
    const auto& [parent, children] = entity.get_component<RelationshipComponent>();

    toml::array children_array = {};
    for (auto child : children)
      children_array.push_back(std::to_string((uint64_t)child));

    const auto table = toml::table{
      {"parent", std::to_string((uint64_t)parent)},
      {"children", children_array}
    };

    entities->push_back(toml::table{{"relationship_component", table}});
  }

  if (entity.has_component<TransformComponent>()) {
    const auto& tc = entity.get_component<TransformComponent>();

    const auto table = toml::table{
      {"position", get_toml_array(tc.position)},
      {"rotation", get_toml_array(tc.rotation)},
      {"scale", get_toml_array(tc.scale)}
    };

    entities->push_back(toml::table{{"transform_component", table}});
  }

  if (entity.has_component<MeshComponent>()) {
    const auto& mrc = entity.get_component<MeshComponent>();

    const auto table = toml::table{
      {"mesh_path", mrc.mesh_base->path},
      {"node_index", mrc.node_index},
      {"cast_shadows", mrc.cast_shadows}
    };

    entities->push_back(toml::table{{"mesh_component", table}});
  }

  if (entity.has_component<LightComponent>()) {
    const auto& light = entity.get_component<LightComponent>();

    const auto table = toml::table{
      {"type", (int)light.type},
      {"use_color_temperature_mode", light.use_color_temperature_mode},
      {"temperature", light.temperature},
      {"color", get_toml_array(light.color)},
      {"intensity", light.intensity},
      {"range", light.range},
      {"cut_off_angle", light.cut_off_angle},
      {"outer_cut_off_angle", light.outer_cut_off_angle},
      {"cast_shadows", light.cast_shadows},
      {"shadow_quality", (int)light.shadow_quality},
    };

    entities->push_back(toml::table{{"light_component", table}});
  }

  if (entity.has_component<SkyLightComponent>()) {
    const auto& light = entity.get_component<SkyLightComponent>();

    const auto table = toml::table{
      {"cubemap_path", light.cubemap ? light.cubemap->get_path() : ""},
      {"intensity", light.intensity},
      {"rotation", light.rotation},
      {"lod_bias", light.lod_bias},
    };

    entities->push_back(toml::table{{"sky_light_component", table}});
  }

  if (entity.has_component<PostProcessProbe>()) {
    const auto& probe = entity.get_component<PostProcessProbe>();

    const auto table = toml::table{
      {"vignette_enabled", probe.vignette_enabled},
      {"vignette_intensity", probe.vignette_intensity},
      {"film_grain_enabled", probe.film_grain_enabled},
      {"film_grain_intensity", probe.film_grain_intensity},
      {"chromatic_aberration_enabled", probe.chromatic_aberration_enabled},
      {"chromatic_aberration_intensity", probe.chromatic_aberration_intensity},
      {"sharpen_enabled", probe.sharpen_enabled},
      {"sharpen_intensity", probe.sharpen_intensity},
    };

    entities->push_back(toml::table{{"post_process_probe", table}});
  }

  if (entity.has_component<CameraComponent>()) {
    const auto& camera = entity.get_component<CameraComponent>();

    // TODO: serialize the rest
    const auto table = toml::table{
      {"fov", camera.camera.get_fov()},
      {"near", camera.camera.get_near()},
      {"far", camera.camera.get_far()},
    };

    entities->push_back(toml::table{{"camera_component", table}});
  }

  // Physics
  if (entity.has_component<RigidbodyComponent>()) {
    const auto& rb = entity.get_component<RigidbodyComponent>();

    const auto table = toml::table{
      {"type", (int)rb.type},
      {"mass", rb.mass},
      {"linear_drag", rb.linear_drag},
      {"angular_drag", rb.angular_drag},
      {"gravity_scale", rb.gravity_scale},
      {"allow_sleep", rb.allow_sleep},
      {"awake", rb.awake},
      {"continuous", rb.continuous},
      {"interpolation", rb.interpolation},
      {"is_sensor", rb.is_sensor},
    };

    entities->push_back(toml::table{{"rigidbody_component", table}});
  }

  if (entity.has_component<BoxColliderComponent>()) {
    const auto& bc = entity.get_component<BoxColliderComponent>();

    const auto table = toml::table{
      {"size", get_toml_array(bc.size)},
      {"offset", get_toml_array(bc.offset)},
      {"density", bc.density},
      {"friction", bc.friction},
      {"restitution", bc.restitution},
    };

    entities->push_back(toml::table{{"box_collider_component", table}});
  }

  if (entity.has_component<SphereColliderComponent>()) {
    const auto& sc = entity.get_component<SphereColliderComponent>();

    const auto table = toml::table{
      {"radius", sc.radius},
      {"offset", get_toml_array(sc.offset)},
      {"density", sc.density},
      {"friction", sc.friction},
      {"restitution", sc.restitution},
    };

    entities->push_back(toml::table{{"sphere_collider_component", table}});
  }

  if (entity.has_component<CapsuleColliderComponent>()) {
    const auto& cc = entity.get_component<CapsuleColliderComponent>();
    const auto table = toml::table{
      {"height", cc.height},
      {"radius", cc.radius},
      {"offset", get_toml_array(cc.offset)},
      {"density", cc.density},
      {"friction", cc.friction},
      {"restitution", cc.restitution},
    };

    entities->push_back(toml::table{{"capsule_collider_component", table}});
  }

  if (entity.has_component<TaperedCapsuleColliderComponent>()) {
    const auto& tcc = entity.get_component<TaperedCapsuleColliderComponent>();

    const auto table = toml::table{
      {"height", tcc.height},
      {"top_radius", tcc.top_radius},
      {"offset", get_toml_array(tcc.offset)},
      {"density", tcc.density},
      {"friction", tcc.friction},
      {"restitution", tcc.restitution},
    };

    entities->push_back(toml::table{{"tapered_capsule_collider_component", table}});
  }

  if (entity.has_component<CylinderColliderComponent>()) {
    const auto& cc = entity.get_component<CylinderColliderComponent>();
    const auto table = toml::table{
      {"height", cc.height},
      {"radius", cc.radius},
      {"offset", get_toml_array(cc.offset)},
      {"density", cc.density},
      {"friction", cc.friction},
      {"restitution", cc.restitution},
    };

    entities->push_back(toml::table{{"cylinder_collider_component", table}});
  }

  if (entity.has_component<MeshColliderComponent>()) {
    const auto& mc = entity.get_component<MeshColliderComponent>();
    const auto table = toml::table{
      {"offset", get_toml_array(mc.offset)},
      {"friction", mc.friction},
      {"restitution", mc.restitution},
    };

    entities->push_back(toml::table{{"mesh_collider_component", table}});
  }

  if (entity.has_component<CharacterControllerComponent>()) {
    const auto& component = entity.get_component<CharacterControllerComponent>();
    const auto table = toml::table{
      {"character_height_standing", component.character_height_standing},
      {"character_radius_standing", component.character_radius_standing},
      {"character_radius_crouching", component.character_radius_crouching},
      {"character_height_crouching", component.character_height_crouching},
      {"control_movement_during_jump", component.control_movement_during_jump},
      {"jump_force", component.jump_force},
      {"friction", component.friction},
      {"collision_tolerance", component.collision_tolerance},
    };

    entities->push_back(toml::table{{"character_controller_component", table}});
  }

  if (entity.has_component<LuaScriptComponent>()) {
    const auto& component = entity.get_component<LuaScriptComponent>();
    const auto& system = component.lua_system;

    const auto table = toml::table{
      {"path", system->get_path()},
    };

    entities->push_back(toml::table{{"lua_script_component", table}});
  }
}

UUID EntitySerializer::deserialize_entity(toml::array* entity_arr, Scene* scene, bool preserve_uuid) {
  // these values are always present
  const uint64_t uuid = std::stoull(entity_arr->get(0)->as_table()->get("uuid")->as_string()->get());
  const auto tag_node = entity_arr->get(1)->as_table()->get("tag_component")->as_table();
  std::string name = tag_node->get("tag")->as_string()->get();

  Entity deserialized_entity;
  if (preserve_uuid)
    deserialized_entity = scene->create_entity_with_uuid(uuid, name);
  else
    deserialized_entity = scene->create_entity(name);

  auto& tag_component = deserialized_entity.get_or_add_component<TagComponent>();
  tag_component.tag = name;
  tag_component.enabled = tag_node->get("enabled")->as_boolean()->get();

  for (auto& ent : *entity_arr) {
    if (const auto relation_node = ent.as_table()->get("relationship_component")) {
      auto& rc = deserialized_entity.get_or_add_component<RelationshipComponent>();
      rc.parent = std::stoull(GET_STRING(relation_node, "parent"));
      const auto children_node = relation_node->as_table()->get("children")->as_array();
      for (auto& child : *children_node)
        rc.children.emplace_back(std::stoull(child.as_string()->get()));
    }
    else if (const auto transform_node = ent.as_table()->get("transform_component")) {
      auto& tc = deserialized_entity.get_or_add_component<TransformComponent>();
      tc.position = get_vec3_toml_array(GET_ARRAY(transform_node, "position"));
      tc.rotation = get_vec3_toml_array(GET_ARRAY(transform_node, "rotation"));
      tc.scale = get_vec3_toml_array(GET_ARRAY(transform_node, "scale"));
    }
    else if (const auto mesh_node = ent.as_table()->get("mesh_component")) {
      auto mesh = AssetManager::get_mesh_asset(GET_STRING(mesh_node, "mesh_path"));
      auto& mc = deserialized_entity.add_component<MeshComponent>(mesh);
      mc.node_index = GET_UINT32(mesh_node, "node_index");
      mc.cast_shadows = GET_BOOL(mesh_node, "cast_shadows");
    }
    else if (const auto light_node = ent.as_table()->get("light_component")) {
      auto& lc = deserialized_entity.add_component<LightComponent>();
      lc.type = (LightComponent::LightType)GET_UINT32(light_node, "type");
      lc.use_color_temperature_mode = GET_BOOL(light_node, "use_color_temperature_mode");
      lc.temperature = GET_UINT32(light_node, "temperature");
      lc.color = get_vec3_toml_array(GET_ARRAY(light_node, "color"));
      lc.intensity = GET_FLOAT(light_node, "intensity");
      lc.range = GET_FLOAT(light_node, "range");
      lc.cut_off_angle = GET_FLOAT(light_node, "cut_off_angle");
      lc.outer_cut_off_angle = GET_FLOAT(light_node, "outer_cut_off_angle");
      lc.cast_shadows = GET_BOOL(light_node, "cast_shadows");
      lc.shadow_quality = (LightComponent::ShadowQualityType)GET_UINT32(light_node, "shadow_quality");
    }
    else if (const auto sky_node = ent.as_table()->get("sky_light_component")) {
      auto& sc = deserialized_entity.add_component<SkyLightComponent>();
      sc.cubemap = AssetManager::get_texture_asset({.path = GET_STRING(sky_node, "path")});
      sc.rotation = GET_FLOAT(sky_node, "rotation");
      sc.intensity = GET_FLOAT(sky_node, "intensity");
      sc.lod_bias = GET_FLOAT(sky_node, "lod_bias");
    }
    else if (const auto pp_node = ent.as_table()->get("post_process_probe")) {
      auto& pp = deserialized_entity.add_component<PostProcessProbe>();
      pp.vignette_enabled = GET_BOOL(pp_node, "vignette_enabled");
      pp.vignette_intensity = GET_FLOAT(pp_node, "vignette_intensity");
      pp.film_grain_enabled = GET_BOOL(pp_node, "film_grain_enabled");
      pp.film_grain_intensity = GET_FLOAT(pp_node, "film_grain_intensity");
      pp.chromatic_aberration_enabled = GET_BOOL(pp_node, "chromatic_aberration_enabled");
      pp.chromatic_aberration_intensity = GET_FLOAT(pp_node, "chromatic_aberration_intensity");
      pp.sharpen_enabled = GET_BOOL(pp_node, "sharpen_enabled");
      pp.sharpen_intensity = GET_FLOAT(pp_node, "sharpen_intensity");
    }
    else if (const auto camera_node = ent.as_table()->get("camera_component")) {
      auto& cc = deserialized_entity.add_component<CameraComponent>();
      cc.camera.set_fov(GET_FLOAT(camera_node, "fov"));
      cc.camera.set_near(GET_FLOAT(camera_node, "near"));
      cc.camera.set_far(GET_FLOAT(camera_node, "far"));
    }
    else if (const auto rb_node = ent.as_table()->get("rigidbody_component")) {
      auto& rb = deserialized_entity.add_component<RigidbodyComponent>();
      rb.type = (RigidbodyComponent::BodyType)GET_UINT32(rb_node, "type");
      rb.mass = GET_FLOAT(rb_node, "mass");
      rb.linear_drag = GET_FLOAT(rb_node, "linear_drag");
      rb.angular_drag = GET_FLOAT(rb_node, "angular_drag");
      rb.gravity_scale = GET_FLOAT(rb_node, "gravity_scale");
      rb.allow_sleep = GET_BOOL(rb_node, "allow_sleep");
      rb.awake = GET_BOOL(rb_node, "awake");
      rb.continuous = GET_BOOL(rb_node, "continuous");
      rb.interpolation = GET_BOOL(rb_node, "interpolation");
      rb.is_sensor = GET_BOOL(rb_node, "is_sensor");
    }
    else if (const auto bc_node = ent.as_table()->get("box_collider_component")) {
      auto& bc = deserialized_entity.add_component<BoxColliderComponent>();
      bc.size = get_vec3_toml_array(GET_ARRAY(bc_node, "size"));
      bc.offset = get_vec3_toml_array(GET_ARRAY(bc_node, "offset"));
      bc.density = GET_FLOAT(bc_node, "density");
      bc.friction = GET_FLOAT(bc_node, "friction");
      bc.restitution = GET_FLOAT(bc_node, "restitution");
    }
    else if (const auto sc_node = ent.as_table()->get("sphere_collider_component")) {
      auto& sc = deserialized_entity.add_component<SphereColliderComponent>();
      sc.radius = GET_FLOAT(sc_node, "radius");
      sc.offset = get_vec3_toml_array(GET_ARRAY(sc_node, "offset"));
      sc.density = GET_FLOAT(sc_node, "density");
      sc.friction = GET_FLOAT(sc_node, "friction");
      sc.restitution = GET_FLOAT(sc_node, "restitution");
    }
    else if (const auto cc_node = ent.as_table()->get("capsule_collider_component")) {
      auto& cc = deserialized_entity.add_component<CapsuleColliderComponent>();
      cc.height = GET_FLOAT(cc_node, "height");
      cc.radius = GET_FLOAT(cc_node, "radius");
      cc.offset = get_vec3_toml_array(GET_ARRAY(cc_node, "offset"));
      cc.density = GET_FLOAT(cc_node, "density");
      cc.friction = GET_FLOAT(cc_node, "friction");
      cc.restitution = GET_FLOAT(cc_node, "restitution");
    }
    else if (const auto tcc_node = ent.as_table()->get("tapered_capsule_collider_component")) {
      auto& tcc = deserialized_entity.add_component<TaperedCapsuleColliderComponent>();
      tcc.height = GET_FLOAT(tcc_node, "height");
      tcc.top_radius = GET_FLOAT(tcc_node, "radius");
      tcc.bottom_radius = GET_FLOAT(tcc_node, "radius");
      tcc.offset = get_vec3_toml_array(GET_ARRAY(tcc_node, "offset"));
      tcc.density = GET_FLOAT(tcc_node, "density");
      tcc.friction = GET_FLOAT(tcc_node, "friction");
      tcc.restitution = GET_FLOAT(tcc_node, "restitution");
    }
    else if (const auto ccc_node = ent.as_table()->get("cylinder_collider_component")) {
      auto& ccc = deserialized_entity.add_component<CylinderColliderComponent>();
      ccc.height = GET_FLOAT(ccc_node, "height");
      ccc.radius = GET_FLOAT(ccc_node, "radius");
      ccc.offset = get_vec3_toml_array(GET_ARRAY(ccc_node, "offset"));
      ccc.density = GET_FLOAT(ccc_node, "density");
      ccc.friction = GET_FLOAT(ccc_node, "friction");
      ccc.restitution = GET_FLOAT(ccc_node, "restitution");
    }
    else if (const auto mc_node = ent.as_table()->get("mesh_collider_component")) {
      auto& mc = deserialized_entity.add_component<MeshColliderComponent>();
      mc.offset = get_vec3_toml_array(GET_ARRAY(mc_node, "offset"));
      mc.friction = GET_FLOAT(mc_node, "friction");
      mc.restitution = GET_FLOAT(mc_node, "restitution");
    }
    else if (const auto chc_node = ent.as_table()->get("character_controller_component")) {
      auto& chc = deserialized_entity.add_component<CharacterControllerComponent>();
      chc.character_height_standing = GET_FLOAT(chc_node, "character_height_standing");
      chc.character_radius_standing = GET_FLOAT(chc_node, "character_radius_standing");
      chc.character_height_crouching = GET_FLOAT(chc_node, "character_height_crouching");
      chc.character_radius_crouching = GET_FLOAT(chc_node, "character_radius_crouching");
      chc.control_movement_during_jump = GET_BOOL(chc_node, "control_movement_during_jump");
      chc.jump_force = GET_FLOAT(chc_node, "jump_force");
      chc.friction = GET_FLOAT(chc_node, "friction");
      chc.collision_tolerance = GET_FLOAT(chc_node, "collision_tolerance");
    }
    else if (const auto lua_node = ent.as_table()->get("lua_script_component")) {
      auto& lsc = deserialized_entity.add_component<LuaScriptComponent>();
      auto path = GET_STRING(lua_node, "path");
      if (!path.empty())
        lsc.lua_system = create_shared<LuaSystem>(path);
    }
  }
  return deserialized_entity.get_uuid();
}

void EntitySerializer::serialize_entity_as_prefab(const char* filepath, Entity entity) {
#if 0 // TODO:
  if (entity.has_component<PrefabComponent>()) {
    OX_CORE_ERROR("Entity already has a prefab component!");
    return;
  }

  ryml::Tree tree;

  ryml::NodeRef node_root = tree.rootref();
  node_root |= ryml::MAP;

  node_root["Prefab"] << entity.add_component_internal<PrefabComponent>().id;

  ryml::NodeRef entities_node = node_root["Entities"];
  entities_node |= ryml::SEQ;

  std::vector<Entity> entities;
  entities.push_back(entity);
  Entity::get_all_children(entity, entities);

  for (const auto& child : entities) {
    if (child)
      serialize_entity(entity.get_scene(), entities_node, child);
  }

  std::stringstream ss;
  ss << tree;
  std::ofstream filestream(filepath);
  filestream << ss.str();
#endif
}

Entity EntitySerializer::deserialize_entity_as_prefab(const char* filepath, Scene* scene) {
#if 0 // TODO:
  auto content = FileUtils::read_file(filepath);
  if (content.empty()) {
    OX_CORE_ERROR("Couldn't read prefab file: {0}", filepath);
  }

  const ryml::Tree tree = ryml::parse_in_arena(ryml::to_csubstr(content));

  if (tree.empty()) {
    OX_CORE_ERROR("Couldn't parse the prefab file {0}", FileSystem::get_file_name(filepath));
  }

  const ryml::ConstNodeRef root = tree.rootref();

  if (!root.has_child("Prefab")) {
    OX_CORE_ERROR("Prefab file doesn't contain a prefab{0}", FileSystem::get_file_name(filepath));
    return {};
  }

  const UUID prefab_id = (uint64_t)root["Prefab"].val().data();

  if (!prefab_id) {
    OX_CORE_ERROR("Invalid prefab ID {0}", FileSystem::get_file_name(filepath));
    return {};
  }

  if (root.has_child("Entities")) {
    const ryml::ConstNodeRef entities_node = root["Entities"];

    Entity root_entity = {};
    std::unordered_map<UUID, UUID> old_new_id_map;
    for (const auto& entity : entities_node) {
      uint64_t old_uuid;
      entity["Entity"] >> old_uuid;
      UUID new_uuid = deserialize_entity(entity, scene, false);
      old_new_id_map.emplace(old_uuid, new_uuid);

      if (!root_entity)
        root_entity = scene->get_entity_by_uuid(new_uuid);
    }

    root_entity.add_component_internal<PrefabComponent>().id = prefab_id;

    // Fix parent/children UUIDs
    for (const auto& [_, newId] : old_new_id_map) {
      auto& relationship_component = scene->get_entity_by_uuid(newId).get_relationship();
      UUID parent = relationship_component.parent;
      if (parent)
        relationship_component.parent = old_new_id_map.at(parent);

      auto& children = relationship_component.children;
      for (auto& id : children)
        id = old_new_id_map.at(id);
    }

    return root_entity;
  }

#endif
  OX_CORE_ERROR("There are not entities in the prefab to deserialize! {0}", FileSystem::get_file_name(filepath));
  return {};
}
}
