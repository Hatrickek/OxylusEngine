#include "EntitySerializer.h"

#include <fstream>

#include "SceneRenderer.h"

#include "Assets/AssetManager.h"
#include "Assets/MaterialSerializer.h"

#include "Core/Entity.h"
#include "Core/YamlHelpers.h"

#include "Render/SceneRendererEvents.h"

#include "Utils/FileUtils.h"

namespace Oxylus {
template <typename T>
void set_enum(const ryml::ConstNodeRef& node, T& data) {
  int type = 0;
  node >> type;
  data = (T)type;
}

void EntitySerializer::serialize_entity(Scene* scene, ryml::NodeRef& entities, Entity entity) {
  ryml::NodeRef entity_node = entities.append_child({ryml::MAP});

  if (entity.has_component<TagComponent>()) {
    const auto& tag = entity.get_component<TagComponent>();

    entity_node["Entity"] << entity.get_uuid();
    auto node = entity_node["TagComponent"];
    node |= ryml::MAP;
    node["Tag"] << tag.tag;
  }

  if (entity.has_component<RelationshipComponent>()) {
    const auto& [Parent, Children] = entity.get_component<RelationshipComponent>();

    auto node = entity_node["RelationshipComponent"];
    node |= ryml::MAP;
    node["Parent"] << Parent;
    node["ChildCount"] << (uint32_t)Children.size();
    auto children_node = node["Children"];
    children_node |= ryml::SEQ;
    for (size_t i = 0; i < Children.size(); i++) {
      children_node.append_child() << Children[i];
    }
  }

  if (entity.has_component<TransformComponent>()) {
    const auto& tc = entity.get_component<TransformComponent>();
    auto node = entity_node["TransformComponent"];
    node |= ryml::MAP;
    auto translation = node["Translation"];
    auto rotation = node["Rotation"];
    auto scale = node["Scale"];
    glm::write(&translation, tc.position);
    glm::write(&rotation, tc.rotation);
    glm::write(&scale, tc.scale);
  }

  if (entity.has_component<MeshComponent>()) {
    const auto& mrc = entity.get_component<MeshComponent>();
    auto node = entity_node["MeshRendererComponent"];
    node |= ryml::MAP;
    node["Mesh"] << mrc.original_mesh->path;
  }

  if (entity.has_component<MaterialComponent>()) {
    const auto& mc = entity.get_component<MaterialComponent>();
    auto node = entity_node["MaterialComponent"];
    node |= ryml::MAP;
    if (mc.using_material_asset)
      node["Path"] << mc.materials[0]->path;
  }

  if (entity.has_component<LightComponent>()) {
    const auto& light = entity.get_component<LightComponent>();
    auto node = entity_node["LightComponent"];
    node |= ryml::MAP;
    auto color_node = node["Color"];
    node["Type"] << (int)light.type;
    node["UseColorTemperatureMode"] << light.use_color_temperature_mode;
    node["Temperature"] << light.temperature;
    glm::write(&color_node, light.color);
    node["Intensity"] << light.intensity;
    node["Range"] << light.range;
    node["CutOffAngle"] << light.cut_off_angle;
    node["OuterCutOffAngle"] << light.outer_cut_off_angle;
    node["ShadowQuality"] << (int)light.shadow_quality;
  }

  if (entity.has_component<SkyLightComponent>()) {
    const auto& light = entity.get_component<SkyLightComponent>();
    auto node = entity_node["SkyLightComponent"];
    node |= ryml::MAP;
    std::string path = light.cubemap ? light.cubemap->get_path() : "";
    node["ImagePath"] << path;
  }

  if (entity.has_component<PostProcessProbe>()) {
    const auto& probe = entity.get_component<PostProcessProbe>();
    auto node = entity_node["PostProcessProbe"];
    node |= ryml::MAP;
    node["VignetteEnabled"] << probe.vignette_enabled;
    node["VignetteIntensity"] << probe.vignette_intensity;

    node["FilmGrainEnabled"] << probe.film_grain_enabled;
    node["FilmGrainIntensity"] << probe.film_grain_intensity;

    node["ChromaticAberrationEnabled"] << probe.chromatic_aberration_enabled;
    node["ChromaticAberrationIntensity"] << probe.chromatic_aberration_intensity;

    node["SharpenEnabled"] << probe.sharpen_enabled;
    node["SharpenIntensity"] << probe.sharpen_intensity;
  }

  if (entity.has_component<CameraComponent>()) {
    const auto& camera = entity.get_component<CameraComponent>();
    auto node = entity_node["CameraComponent"];
    node |= ryml::MAP;
    node["FOV"] << camera.system->get_fov();
    node["NearClip"] << camera.system->get_near();
    node["FarClip"] << camera.system->get_far();
  }

  // Physics
  if (entity.has_component<RigidbodyComponent>()) {
    auto node = entity_node["RigidbodyComponent"];
    node |= ryml::MAP;

    const auto& rb = entity.get_component<RigidbodyComponent>();
    node["Type"] << static_cast<int>(rb.type);
    node["Mass"] << rb.mass;
    node["LinearDrag"] << rb.linear_drag;
    node["AngularDrag"] << rb.angular_drag;
    node["GravityScale"] << rb.gravity_scale;
    node["AllowSleep"] << rb.allow_sleep;
    node["Awake"] << rb.awake;
    node["Continuous"] << rb.continuous;
    node["Interpolation"] << rb.interpolation;
    node["IsSensor"] << rb.is_sensor;
  }

  if (entity.has_component<BoxColliderComponent>()) {
    auto node = entity_node["BoxColliderComponent"];
    node |= ryml::MAP;

    const auto& bc = entity.get_component<BoxColliderComponent>();
    node["Size"] << bc.size;
    node["Offset"] << bc.offset;
    node["Density"] << bc.density;
    node["Friction"] << bc.friction;
    node["Restitution"] << bc.restitution;
  }

  if (entity.has_component<SphereColliderComponent>()) {
    auto node = entity_node["SphereColliderComponent"];
    node |= ryml::MAP;

    const auto& sc = entity.get_component<SphereColliderComponent>();
    node["Radius"] << sc.radius;
    node["Offset"] << sc.offset;
    node["Density"] << sc.density;
    node["Friction"] << sc.friction;
    node["Restitution"] << sc.restitution;
  }

  if (entity.has_component<CapsuleColliderComponent>()) {
    auto node = entity_node["CapsuleColliderComponent"];
    node |= ryml::MAP;

    const auto& cc = entity.get_component<CapsuleColliderComponent>();
    node["Height"] << cc.height;
    node["Radius"] << cc.radius;
    node["Offset"] << cc.offset;
    node["Density"] << cc.density;
    node["Friction"] << cc.friction;
    node["Restitution"] << cc.restitution;
  }

  if (entity.has_component<TaperedCapsuleColliderComponent>()) {
    auto node = entity_node["TaperedCapsuleColliderComponent"];
    node |= ryml::MAP;

    const auto& tcc = entity.get_component<TaperedCapsuleColliderComponent>();
    node["Height"] << tcc.height;
    node["TopRadius"] << tcc.top_radius;
    node["BottomRadius"] << tcc.bottom_radius;
    node["Offset"] << tcc.offset;
    node["Density"] << tcc.density;
    node["Friction"] << tcc.friction;
    node["Restitution"] << tcc.restitution;
  }

  if (entity.has_component<CylinderColliderComponent>()) {
    auto node = entity_node["CylinderColliderComponent"];
    node |= ryml::MAP;

    const auto& cc = entity.get_component<CapsuleColliderComponent>();
    node["Height"] << cc.height;
    node["Radius"] << cc.radius;
    node["Offset"] << cc.offset;
    node["Density"] << cc.density;
    node["Friction"] << cc.friction;
    node["Restitution"] << cc.restitution;
  }

  if (entity.has_component<CharacterControllerComponent>()) {
    auto node = entity_node["CharacterControllerComponent"];
    node |= ryml::MAP;

    const auto& component = entity.get_component<CharacterControllerComponent>();

    // Size
    node["CharacterHeightStanding"] << component.character_height_standing;
    node["CharacterRadiusStanding"] << component.character_radius_standing;
    node["CharacterRadiusCrouching"] << component.character_radius_crouching;
    node["CharacterHeightCrouching"] << component.character_height_crouching;

    // Movement
    node["ControlMovementDuringJump"] << component.control_movement_during_jump;
    node["JumpForce"] << component.jump_force;

    node["Friction"] << component.friction;
    node["CollisionTolerance"] << component.collision_tolerance;
  }

  if (entity.has_component<LuaScriptComponent>()) {
    auto node = entity_node["LuaScriptComponent"];
    node |= ryml::MAP;

    const auto& component = entity.get_component<LuaScriptComponent>();
    const auto& system = component.lua_system;

    node["Path"] << system->get_path();
  }

  if (entity.has_component<CustomComponent>()) {
    auto node = entity_node["CustomComponent"];
    node |= ryml::MAP;

    const auto& component = entity.get_component<CustomComponent>();

    node["Name"] << component.name;
    auto fields_node = node["Fields"];
    fields_node |= ryml::MAP;
    for (const auto& field : component.fields) {
      auto field_node = fields_node[field.name.c_str()];
      field_node |= ryml::MAP;
      field_node["Type"] << (int)field.type;
      field_node["Value"] << field.value;
    }
  }
}

UUID EntitySerializer::deserialize_entity(ryml::ConstNodeRef entity_node, Scene* scene, bool preserve_uuid) {
  const auto st = std::string(entity_node["Entity"].val().data());
  const uint64_t uuid = std::stoull(st.substr(0, st.find('\n')));

  std::string name;
  if (entity_node.has_child("TagComponent"))
    entity_node["TagComponent"]["Tag"] >> name;

  Entity deserialized_entity;
  if (preserve_uuid)
    deserialized_entity = scene->create_entity_with_uuid(uuid, name);
  else
    deserialized_entity = scene->create_entity(name);

  if (entity_node.has_child("TransformComponent")) {
    auto& tc = deserialized_entity.get_component<TransformComponent>();
    const auto& node = entity_node["TransformComponent"];

    glm::read(node["Translation"], &tc.position);
    glm::read(node["Rotation"], &tc.rotation);
    glm::read(node["Scale"], &tc.scale);
  }

  if (entity_node.has_child("RelationshipComponent")) {
    auto& rc = deserialized_entity.get_component<RelationshipComponent>();
    const auto node = entity_node["RelationshipComponent"];
    uint64_t parent_id = 0;
    node["Parent"] >> parent_id;
    rc.parent = parent_id;

    size_t child_count = 0;
    node["ChildCount"] >> child_count;
    rc.children.clear();
    rc.children.reserve(child_count);
    const auto children = node["Children"];

    if (children.num_children() == child_count) {
      for (size_t i = 0; i < child_count; i++) {
        uint64_t child_id = 0;
        children[i] >> child_id;
        UUID child = child_id;
        if (child)
          rc.children.push_back(child);
      }
    }
  }

  if (entity_node.has_child("MeshRendererComponent")) {
    const auto& node = entity_node["MeshRendererComponent"];

    std::string mesh_path;
    node["Mesh"] >> mesh_path;
  }

  if (entity_node.has_child("MaterialComponent")) {
    auto& mc = deserialized_entity.add_component_internal<MaterialComponent>();

    const auto& node = entity_node["MaterialComponent"];

    std::string asset_path;
    if (node.has_child("Path"))
      node["Path"] >> asset_path;

    if (!asset_path.empty()) {
      mc.materials.clear();
      auto mat = mc.materials.emplace_back(create_ref<Material>());
      mat->create();
      MaterialSerializer serializer(mat);
      serializer.deserialize(asset_path);
      mc.using_material_asset = true;
    }
  }

  if (entity_node.has_child("LightComponent")) {
    auto& light = deserialized_entity.add_component_internal<LightComponent>();
    const auto& node = entity_node["LightComponent"];

    set_enum(node["Type"], light.type);
    node["UseColorTemperatureMode"] >> light.use_color_temperature_mode;
    node["Temperature"] >> light.temperature;
    node["Intensity"] >> light.intensity;
    glm::read(node["Color"], &light.color);
    node["Range"] >> light.range;
    node["CutOffAngle"] >> light.cut_off_angle;
    node["OuterCutOffAngle"] >> light.outer_cut_off_angle;
    set_enum(node["ShadowQuality"], light.shadow_quality);
  }

  if (entity_node.has_child("SkyLightComponent")) {
    auto& light = deserialized_entity.add_component_internal<SkyLightComponent>();
    const auto& node = entity_node["SkyLightComponent"];

    std::string path{};
    node["ImagePath"] >> path;
    if (!path.empty()) {
      light.cubemap = AssetManager::get_texture_asset({.path = path});
      scene->get_renderer()->dispatcher.trigger(SkyboxLoadEvent{light.cubemap});
    }
  }

  if (entity_node.has_child("PostProcessProbe")) {
    auto& probe = deserialized_entity.add_component_internal<PostProcessProbe>();
    const auto& node = entity_node["PostProcessProbe"];

    node["VignetteEnabled"] >> probe.vignette_enabled;
    node["VignetteIntensity"] >> probe.vignette_intensity;

    node["FilmGrainEnabled"] >> probe.film_grain_enabled;
    node["FilmGrainIntensity"] >> probe.film_grain_intensity;

    node["ChromaticAberrationEnabled"] >> probe.chromatic_aberration_enabled;
    node["ChromaticAberrationIntensity"] >> probe.chromatic_aberration_intensity;

    node["SharpenEnabled"] >> probe.sharpen_enabled;
    node["SharpenIntensity"] >> probe.sharpen_intensity;
  }

  if (entity_node.has_child("CameraComponent")) {
    auto& camera = deserialized_entity.add_component_internal<CameraComponent>();
    const auto& node = entity_node["CameraComponent"];

    float fov = 0;
    float nearclip = 0;
    float farclip = 0;
    node["FOV"] >> fov;
    node["NearClip"] >> nearclip;
    node["FarClip"] >> farclip;

    camera.system->set_fov(fov);
    camera.system->set_near(nearclip);
    camera.system->set_far(farclip);
  }

  if (entity_node.has_child("RigidbodyComponent")) {
    auto& rb = deserialized_entity.add_component_internal<RigidbodyComponent>();
    const auto node = entity_node["RigidbodyComponent"];

    set_enum(node["Type"], rb.type);
    node["Mass"] >> rb.mass;
    node["LinearDrag"] >> rb.linear_drag;
    node["AngularDrag"] >> rb.angular_drag;
    node["GravityScale"] >> rb.gravity_scale;
    node["AllowSleep"] >> rb.allow_sleep;
    node["Awake"] >> rb.awake;
    node["Continuous"] >> rb.continuous;
    node["Interpolation"] >> rb.interpolation;
    node["IsSensor"] >> rb.is_sensor;
  }

  if (entity_node.has_child("BoxColliderComponent")) {
    const auto node = entity_node["BoxColliderComponent"];
    auto& bc = deserialized_entity.add_component_internal<BoxColliderComponent>();

    node["Size"] >> bc.size;
    glm::read(node["Offset"], &bc.offset);
    node["Density"] >> bc.density;
    node["Friction"] >> bc.friction;
    node["Restitution"] >> bc.restitution;
  }

  if (entity_node.has_child("SphereColliderComponent")) {
    auto node = entity_node["SphereColliderComponent"];

    auto& sc = deserialized_entity.add_component_internal<SphereColliderComponent>();
    node["Radius"] >> sc.radius;
    glm::read(node["Offset"], &sc.offset);
    node["Offset"] >> sc.offset;
    node["Density"] >> sc.density;
    node["Friction"] >> sc.friction;
    node["Restitution"] >> sc.restitution;
  }

  if (entity_node.has_child("CapsuleColliderComponent")) {
    auto node = entity_node["CapsuleColliderComponent"];

    auto& cc = deserialized_entity.add_component_internal<CapsuleColliderComponent>();
    node["Height"] >> cc.height;
    node["Radius"] >> cc.radius;
    glm::read(node["Offset"], &cc.offset);
    node["Density"] >> cc.density;
    node["Friction"] >> cc.friction;
    node["Restitution"] >> cc.restitution;
  }

  if (entity_node.has_child("TaperedCapsuleColliderComponent")) {
    const auto node = entity_node["TaperedCapsuleColliderComponent"];

    auto& tcc = deserialized_entity.add_component_internal<TaperedCapsuleColliderComponent>();
    node["Height"] >> tcc.height;
    node["TopRadius"] >> tcc.top_radius;
    node["BottomRadius"] >> tcc.bottom_radius;
    glm::read(node["Offset"], &tcc.offset);
    node["Density"] >> tcc.density;
    node["Friction"] >> tcc.friction;
    node["Restitution"] >> tcc.restitution;
  }

  if (entity_node.has_child("CylinderColliderComponent")) {
    auto node = entity_node["CylinderColliderComponent"];

    auto& cc = deserialized_entity.add_component_internal<CapsuleColliderComponent>();
    node["Height"] >> cc.height;
    node["Radius"] >> cc.radius;
    glm::read(node["Offset"], &cc.offset);
    node["Density"] >> cc.density;
    node["Friction"] >> cc.friction;
    node["Restitution"] >> cc.restitution;
  }

  if (entity_node.has_child("CharacterControllerComponent")) {
    auto& component = deserialized_entity.add_component_internal<CharacterControllerComponent>();
    const auto& node = entity_node["CharacterControllerComponent"];

    // Size
    node["CharacterHeightStanding"] >> component.character_height_standing;
    node["CharacterRadiusStanding"] >> component.character_radius_standing;
    node["CharacterRadiusCrouching"] >> component.character_radius_crouching;
    node["CharacterHeightCrouching"] >> component.character_height_crouching;

    // Movement
    node["ControlMovementDuringJump"] >> component.control_movement_during_jump;
    node["JumpForce"] >> component.jump_force;

    node["Friction"] >> component.friction;
    node["CollisionTolerance"] >> component.collision_tolerance;
  }

  if (entity_node.has_child("LuaScriptComponent")) {
    auto& component = deserialized_entity.add_component_internal<LuaScriptComponent>();
    const auto& node = entity_node["LuaScriptComponent"];

    // Size
    std::string path = {};
    node["Path"] >> path;

    if (!path.empty()) {
      component.lua_system = create_ref<LuaSystem>(path);
    }
  }

  if (entity_node.has_child("CustomComponent")) {
    auto& component = deserialized_entity.add_component_internal<CustomComponent>();
    const auto& node = entity_node["CustomComponent"];

    node["Name"] >> component.name;
    auto fields_node = node["Fields"];
    for (size_t i = 0; i < fields_node.num_children(); i++) {
      CustomComponent::ComponentField field;
      auto key = std::string(fields_node[i].get()->m_key.scalar.data());
      field.name = key.substr(0, key.find(':'));
      set_enum(fields_node[i]["Type"], field.type);
      fields_node[i]["Value"] >> field.value;
      component.fields.emplace_back(field);
    }
  }

  return deserialized_entity.get_uuid();
}

void EntitySerializer::serialize_entity_as_prefab(const char* filepath, Entity entity) {
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
}

Entity EntitySerializer::deserialize_entity_as_prefab(const char* filepath, Scene* scene) {
  auto content = FileUtils::read_file(filepath);
  if (content.empty()) {
    OX_CORE_ERROR("Couldn't read prefab file: {0}", filepath);

    // Try to read it again from assets path
    content = FileUtils::read_file(AssetManager::get_asset_file_system_path(filepath).string());
    if (!content.empty())
      OX_CORE_INFO("Could load the prefab file from assets path: {0}", filepath);
    else {
      return {};
    }
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

  OX_CORE_ERROR("There are not entities in the prefab to deserialize! {0}", FileSystem::get_file_name(filepath));
  return {};
}
}
