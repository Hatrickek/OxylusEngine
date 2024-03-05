#include "EntitySerializer.h"

#include <filesystem>
#include <fstream>

#include "Scene.h"
#include "SceneRenderer.h"

#include "Assets/AssetManager.h"

#include "Core/App.h"

#include "Scene/Entity.h"

#include "Utils/Log.h"

namespace Ox {
#define GET_STRING(node, component, name) component.name =  node->as_table()->get(#name)->as_string()->get()
#define GET_STRING2(node, name) node->as_table()->get(name)->as_string()->get()
#define GET_FLOAT(node, component, name) component.name = (float)node->as_table()->get(#name)->as_floating_point()->get()
#define GET_FLOAT2(node, name) (float)node->as_table()->get(name)->as_floating_point()->get()
#define GET_UINT32(node, component, name) component.name = (uint32_t)node->as_table()->get(#name)->as_integer()->get()
#define GET_UINT322(node, name) (uint32_t)node->as_table()->get(name)->as_integer()->get()
#define GET_BOOL(node, component, name) component.name = node->as_table()->get(#name)->as_boolean()->get()
#define GET_BOOL2(node, name) node->as_table()->get(name)->as_boolean()->get()
#define GET_ARRAY(node, name) node->as_table()->get(name)->as_array()

void EntitySerializer::serialize_entity(toml::array* entities, Scene* scene, Entity entity) {
  entities->push_back(toml::table{{"uuid", std::to_string((uint64_t)EUtil::get_uuid(scene->registry, entity))}});

  if (scene->registry.all_of<TagComponent>(entity)) {
    const auto& tag = scene->registry.get<TagComponent>(entity);

    const auto table = toml::table{
      {"tag", tag.tag},
      {"enabled", tag.enabled}
    };

    entities->push_back(toml::table{{"tag_component", table}});
  }

  if (scene->registry.all_of<RelationshipComponent>(entity)) {
    const auto& [parent, children] = scene->registry.get<RelationshipComponent>(entity);

    toml::array children_array = {};
    for (auto child : children)
      children_array.push_back(std::to_string((uint64_t)child));

    const auto table = toml::table{
      {"parent", std::to_string((uint64_t)parent)},
      {"children", children_array}
    };

    entities->push_back(toml::table{{"relationship_component", table}});
  }

  if (scene->registry.all_of<TransformComponent>(entity)) {
    const auto& tc = scene->registry.get<TransformComponent>(entity);

    const auto table = toml::table{
      {"position", get_toml_array(tc.position)},
      {"rotation", get_toml_array(tc.rotation)},
      {"scale", get_toml_array(tc.scale)}
    };

    entities->push_back(toml::table{{"transform_component", table}});
  }

  if (scene->registry.all_of<MeshComponent>(entity)) {
    const auto& mrc = scene->registry.get<MeshComponent>(entity);

    const auto table = toml::table{
      {"mesh_path", App::get_relative(mrc.mesh_base->path)},
      {"node_index", mrc.node_index},
      {"cast_shadows", mrc.cast_shadows}
    };

    entities->push_back(toml::table{{"mesh_component", table}});
  }

  if (scene->registry.all_of<LightComponent>(entity)) {
    const auto& light = scene->registry.get<LightComponent>(entity);

    const auto table = toml::table{
      {"type", (int)light.type},
      {"color_temperature_mode", light.color_temperature_mode},
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

  if (scene->registry.all_of<SkyLightComponent>(entity)) {
    const auto& light = scene->registry.get<SkyLightComponent>(entity);

    const auto table = toml::table{
      {"cubemap_path", light.cubemap ? App::get_relative(light.cubemap->get_path()) : ""},
      {"intensity", light.intensity},
      {"rotation", light.rotation},
      {"lod_bias", light.lod_bias},
    };

    entities->push_back(toml::table{{"sky_light_component", table}});
  }

  if (scene->registry.all_of<PostProcessProbe>(entity)) {
    const auto& probe = scene->registry.get<PostProcessProbe>(entity);

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

  if (scene->registry.all_of<CameraComponent>(entity)) {
    const auto& camera = scene->registry.get<CameraComponent>(entity);

    // TODO: serialize the rest
    const auto table = toml::table{
      {"fov", camera.camera.get_fov()},
      {"near", camera.camera.get_near()},
      {"far", camera.camera.get_far()},
    };

    entities->push_back(toml::table{{"camera_component", table}});
  }

  // Physics
  if (scene->registry.all_of<RigidbodyComponent>(entity)) {
    const auto& rb = scene->registry.get<RigidbodyComponent>(entity);

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

  if (scene->registry.all_of<BoxColliderComponent>(entity)) {
    const auto& bc = scene->registry.get<BoxColliderComponent>(entity);

    const auto table = toml::table{
      {"size", get_toml_array(bc.size)},
      {"offset", get_toml_array(bc.offset)},
      {"density", bc.density},
      {"friction", bc.friction},
      {"restitution", bc.restitution},
    };

    entities->push_back(toml::table{{"box_collider_component", table}});
  }

  if (scene->registry.all_of<SphereColliderComponent>(entity)) {
    const auto& sc = scene->registry.get<SphereColliderComponent>(entity);

    const auto table = toml::table{
      {"radius", sc.radius},
      {"offset", get_toml_array(sc.offset)},
      {"density", sc.density},
      {"friction", sc.friction},
      {"restitution", sc.restitution},
    };

    entities->push_back(toml::table{{"sphere_collider_component", table}});
  }

  if (scene->registry.all_of<CapsuleColliderComponent>(entity)) {
    const auto& cc = scene->registry.get<CapsuleColliderComponent>(entity);
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

  if (scene->registry.all_of<TaperedCapsuleColliderComponent>(entity)) {
    const auto& tcc = scene->registry.get<TaperedCapsuleColliderComponent>(entity);

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

  if (scene->registry.all_of<CylinderColliderComponent>(entity)) {
    const auto& cc = scene->registry.get<CylinderColliderComponent>(entity);
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

  if (scene->registry.all_of<MeshColliderComponent>(entity)) {
    const auto& mc = scene->registry.get<MeshColliderComponent>(entity);
    const auto table = toml::table{
      {"offset", get_toml_array(mc.offset)},
      {"friction", mc.friction},
      {"restitution", mc.restitution},
    };

    entities->push_back(toml::table{{"mesh_collider_component", table}});
  }

  if (scene->registry.all_of<CharacterControllerComponent>(entity)) {
    const auto& component = scene->registry.get<CharacterControllerComponent>(entity);
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

  if (scene->registry.all_of<LuaScriptComponent>(entity)) {
    const auto& component = scene->registry.get<LuaScriptComponent>(entity);
    const auto& systems = component.lua_systems;

    toml::array path_array = {};
    for (auto system : systems)
      path_array.push_back(system->get_path());

    const auto table = toml::table{
      {"paths", path_array}
    };

    entities->push_back(toml::table{{"lua_script_component", table}});
  }
}

UUID EntitySerializer::deserialize_entity(toml::array* entity_arr, Scene* scene, bool preserve_uuid) {
  // these values are always present
  const uint64_t uuid = std::stoull(entity_arr->get(0)->as_table()->get("uuid")->as_string()->get());
  const auto tag_node = entity_arr->get(1)->as_table()->get("tag_component")->as_table();
  std::string name = tag_node->get("tag")->as_string()->get();

  entt::entity deserialized_entity = {};
  if (preserve_uuid)
    deserialized_entity = scene->create_entity_with_uuid(uuid, name);
  else
    deserialized_entity = scene->create_entity(name);

  auto& reg = scene->registry;

  auto& tag_component = reg.get_or_emplace<TagComponent>(deserialized_entity);
  tag_component.tag = name;
  tag_component.enabled = tag_node->get("enabled")->as_boolean()->get();

  for (auto& ent : *entity_arr) {
    if (const auto relation_node = ent.as_table()->get("relationship_component")) {
      auto& rc = reg.get_or_emplace<RelationshipComponent>(deserialized_entity);
      rc.parent = std::stoull(GET_STRING2(relation_node, "parent"));
      const auto children_node = relation_node->as_table()->get("children")->as_array();
      for (auto& child : *children_node)
        rc.children.emplace_back(std::stoull(child.as_string()->get()));
    }
    else if (const auto transform_node = ent.as_table()->get("transform_component")) {
      auto& tc = reg.get_or_emplace<TransformComponent>(deserialized_entity);
      tc.position = get_vec3_toml_array(GET_ARRAY(transform_node, "position"));
      tc.rotation = get_vec3_toml_array(GET_ARRAY(transform_node, "rotation"));
      tc.scale = get_vec3_toml_array(GET_ARRAY(transform_node, "scale"));
    }
    else if (const auto mesh_node = ent.as_table()->get("mesh_component")) {
      const auto path = App::get_absolute(GET_STRING2(mesh_node, "mesh_path"));
      auto mesh = AssetManager::get_mesh_asset(path);
      auto& mc = reg.emplace<MeshComponent>(deserialized_entity, mesh);
      GET_UINT32(mesh_node, mc, node_index);
      GET_BOOL(mesh_node, mc, cast_shadows);
    }
    else if (const auto light_node = ent.as_table()->get("light_component")) {
      auto& lc = reg.emplace<LightComponent>(deserialized_entity);
      lc.type = (LightComponent::LightType)GET_UINT322(light_node, "type");
      GET_BOOL(light_node, lc, color_temperature_mode);
      GET_UINT32(light_node, lc, temperature);
      lc.color = get_vec3_toml_array(GET_ARRAY(light_node, "color"));
      GET_FLOAT(light_node, lc, intensity);
      GET_FLOAT(light_node, lc, range);
      GET_FLOAT(light_node, lc, cut_off_angle);
      GET_FLOAT(light_node, lc, outer_cut_off_angle);
      GET_BOOL(light_node, lc, cast_shadows);
      lc.shadow_quality = (LightComponent::ShadowQualityType)GET_UINT322(light_node, "shadow_quality");
    }
    else if (const auto sky_node = ent.as_table()->get("sky_light_component")) {
      auto& sc = reg.emplace<SkyLightComponent>(deserialized_entity);
      auto path = App::get_absolute(GET_STRING2(sky_node, "path"));
      sc.cubemap = AssetManager::get_texture_asset({.path = path});
      GET_FLOAT(sky_node, sc, rotation);
      GET_FLOAT(sky_node, sc, intensity);
      GET_FLOAT(sky_node, sc, lod_bias);
    }
    else if (const auto pp_node = ent.as_table()->get("post_process_probe")) {
      auto& pp = reg.emplace<PostProcessProbe>(deserialized_entity);
      GET_BOOL(pp_node, pp, vignette_enabled);
      GET_FLOAT(pp_node, pp, vignette_intensity);
      GET_BOOL(pp_node, pp, film_grain_enabled);
      GET_FLOAT(pp_node, pp, film_grain_intensity);
      GET_BOOL(pp_node, pp, chromatic_aberration_enabled);
      GET_FLOAT(pp_node, pp, chromatic_aberration_intensity);
      GET_BOOL(pp_node, pp, sharpen_enabled);
      GET_FLOAT(pp_node, pp, sharpen_intensity);
    }
    else if (const auto camera_node = ent.as_table()->get("camera_component")) {
      auto& cc = reg.emplace<CameraComponent>(deserialized_entity);
      cc.camera.set_fov(GET_FLOAT2(camera_node, "fov"));
      cc.camera.set_near(GET_FLOAT2(camera_node, "near"));
      cc.camera.set_far(GET_FLOAT2(camera_node, "far"));
    }
    else if (const auto rb_node = ent.as_table()->get("rigidbody_component")) {
      auto& rb = reg.emplace<RigidbodyComponent>(deserialized_entity);
      rb.type = (RigidbodyComponent::BodyType)GET_UINT322(rb_node, "type");
      GET_FLOAT(rb_node, rb, mass);
      GET_FLOAT(rb_node, rb, linear_drag);
      GET_FLOAT(rb_node, rb, angular_drag);
      GET_FLOAT(rb_node, rb, gravity_scale);
      GET_BOOL(rb_node, rb, allow_sleep);
      GET_BOOL(rb_node, rb, awake);
      GET_BOOL(rb_node, rb, continuous);
      GET_BOOL(rb_node, rb, interpolation);
      GET_BOOL(rb_node, rb, is_sensor);
    }
    else if (const auto bc_node = ent.as_table()->get("box_collider_component")) {
      auto& bc = reg.emplace<BoxColliderComponent>(deserialized_entity);
      bc.size = get_vec3_toml_array(GET_ARRAY(bc_node, "size"));
      bc.offset = get_vec3_toml_array(GET_ARRAY(bc_node, "offset"));
      GET_FLOAT(bc_node, bc, density);
      GET_FLOAT(bc_node, bc, friction);
      GET_FLOAT(bc_node, bc, restitution);
    }
    else if (const auto sc_node = ent.as_table()->get("sphere_collider_component")) {
      auto& sc = reg.emplace<SphereColliderComponent>(deserialized_entity);
      GET_FLOAT(sc_node, sc, radius);
      sc.offset = get_vec3_toml_array(GET_ARRAY(sc_node, "offset"));
      GET_FLOAT(sc_node, sc, density);
      GET_FLOAT(sc_node, sc, friction);
      GET_FLOAT(sc_node, sc, restitution);
    }
    else if (const auto cc_node = ent.as_table()->get("capsule_collider_component")) {
      auto& cc = reg.emplace<CapsuleColliderComponent>(deserialized_entity);
      GET_FLOAT(cc_node, cc, height);
      GET_FLOAT(cc_node, cc, radius);
      cc.offset = get_vec3_toml_array(GET_ARRAY(cc_node, "offset"));
      GET_FLOAT(cc_node, cc, density);
      GET_FLOAT(cc_node, cc, friction);
      GET_FLOAT(cc_node, cc, restitution);
    }
    else if (const auto tcc_node = ent.as_table()->get("tapered_capsule_collider_component")) {
      auto& tcc = reg.emplace<TaperedCapsuleColliderComponent>(deserialized_entity);
      GET_FLOAT(tcc_node, tcc, height);
      GET_FLOAT(tcc_node, tcc, top_radius);
      GET_FLOAT(tcc_node, tcc, bottom_radius);
      tcc.offset = get_vec3_toml_array(GET_ARRAY(tcc_node, "offset"));
      GET_FLOAT(tcc_node, tcc, density);
      GET_FLOAT(tcc_node, tcc, friction);
      GET_FLOAT(tcc_node, tcc, restitution);
    }
    else if (const auto ccc_node = ent.as_table()->get("cylinder_collider_component")) {
      auto& ccc = reg.emplace<CylinderColliderComponent>(deserialized_entity);
      GET_FLOAT(ccc_node, ccc, height);
      GET_FLOAT(ccc_node, ccc, radius);
      ccc.offset = get_vec3_toml_array(GET_ARRAY(ccc_node, "offset"));
      GET_FLOAT(ccc_node, ccc, density);
      GET_FLOAT(ccc_node, ccc, friction);
      GET_FLOAT(ccc_node, ccc, restitution);
    }
    else if (const auto mc_node = ent.as_table()->get("mesh_collider_component")) {
      auto& mc = reg.emplace<MeshColliderComponent>(deserialized_entity);
      mc.offset = get_vec3_toml_array(GET_ARRAY(mc_node, "offset"));
      GET_FLOAT(mc_node, mc, friction);
      GET_FLOAT(mc_node, mc, restitution);
    }
    else if (const auto chc_node = ent.as_table()->get("character_controller_component")) {
      auto& chc = reg.emplace<CharacterControllerComponent>(deserialized_entity);
      GET_FLOAT(chc_node, chc, character_height_standing);
      GET_FLOAT(chc_node, chc, character_radius_standing);
      GET_FLOAT(chc_node, chc, character_height_crouching);
      GET_FLOAT(chc_node, chc, character_radius_crouching);
      GET_BOOL(chc_node, chc, control_movement_during_jump);
      GET_FLOAT(chc_node, chc, jump_force);
      GET_FLOAT(chc_node, chc, friction);
      GET_FLOAT(chc_node, chc, collision_tolerance);
    }
    else if (const auto lua_node = ent.as_table()->get("lua_script_component")) {
      auto& lsc = reg.emplace<LuaScriptComponent>(deserialized_entity);
      auto paths = GET_ARRAY(lua_node, "paths");
      for (auto& path : *paths) {
        auto ab = App::get_absolute(path.as_string()->get());
        lsc.lua_systems.emplace_back(create_shared<LuaSystem>(ab));
      }
    }
  }
  return EUtil::get_uuid(reg, deserialized_entity);
}

void EntitySerializer::serialize_entity_as_prefab(const char* filepath, Entity entity) {
#if 0 // TODO:
  if (scene->registry.all_of<PrefabComponent>(entity)) {
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
