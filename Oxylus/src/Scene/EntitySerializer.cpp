#include "EntitySerializer.h"

#include <fstream>

#include "Assets/AssetManager.h"
#include "Assets/MaterialSerializer.h"

#include "Core/Entity.h"
#include "Core/YamlHelpers.h"

#include "Utils/FileUtils.h"

namespace Oxylus {
template <typename T>
void SetEnum(const ryml::ConstNodeRef& node, T& data) {
  int type = 0;
  node >> type;
  data = (T)type;
}

void EntitySerializer::SerializeEntity(Scene* scene, ryml::NodeRef& entities, Entity entity) {
  ryml::NodeRef entityNode = entities.append_child({ryml::MAP});

  if (entity.has_component<TagComponent>()) {
    const auto& tag = entity.get_component<TagComponent>();

    entityNode["Entity"] << entity.get_uuid();
    auto node = entityNode["TagComponent"];
    node |= ryml::MAP;
    node["Tag"] << tag.tag;
  }

  if (entity.has_component<RelationshipComponent>()) {
    const auto& [Parent, Children] = entity.get_component<RelationshipComponent>();

    auto node = entityNode["RelationshipComponent"];
    node |= ryml::MAP;
    node["Parent"] << Parent;
    node["ChildCount"] << (uint32_t)Children.size();
    auto childrenNode = node["Children"];
    childrenNode |= ryml::SEQ;
    for (size_t i = 0; i < Children.size(); i++) {
      childrenNode.append_child() << Children[i];
    }
  }

  if (entity.has_component<TransformComponent>()) {
    const auto& tc = entity.get_component<TransformComponent>();
    auto node = entityNode["TransformComponent"];
    node |= ryml::MAP;
    auto translation = node["Translation"];
    auto rotation = node["Rotation"];
    auto scale = node["Scale"];
    glm::write(&translation, tc.translation);
    glm::write(&rotation, tc.rotation);
    glm::write(&scale, tc.scale);
  }

  if (entity.has_component<MeshRendererComponent>()) {
    const auto& mrc = entity.get_component<MeshRendererComponent>();
    auto node = entityNode["MeshRendererComponent"];
    node |= ryml::MAP;
    node["Mesh"] << mrc.mesh_geometry->path;
    node["SubmeshIndex"] << mrc.submesh_index;
  }

  if (entity.has_component<MaterialComponent>()) {
    const auto& mc = entity.get_component<MaterialComponent>();
    auto node = entityNode["MaterialComponent"];
    node |= ryml::MAP;
    if (mc.using_material_asset)
      node["Path"] << mc.materials[0]->path;
  }

  if (entity.has_component<LightComponent>()) {
    const auto& light = entity.get_component<LightComponent>();
    auto node = entityNode["LightComponent"];
    node |= ryml::MAP;
    auto colorNode = node["Color"];
    node["Type"] << (int)light.type;
    node["UseColorTemperatureMode"] << light.use_color_temperature_mode;
    node["Temperature"] << light.temperature;
    glm::write(&colorNode, light.color);
    node["Intensity"] << light.intensity;
    node["Range"] << light.range;
    node["CutOffAngle"] << light.cut_off_angle;
    node["OuterCutOffAngle"] << light.outer_cut_off_angle;
    node["ShadowQuality"] << (int)light.shadow_quality;
  }

  if (entity.has_component<SkyLightComponent>()) {
    const auto& light = entity.get_component<SkyLightComponent>();
    auto node = entityNode["SkyLightComponent"];
    node |= ryml::MAP;
    std::string path = light.cubemap ? light.cubemap->get_path() : "";
    node["ImagePath"] << path;
  }

  if (entity.has_component<PostProcessProbe>()) {
    const auto& probe = entity.get_component<PostProcessProbe>();
    auto node = entityNode["PostProcessProbe"];
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
    auto node = entityNode["CameraComponent"];
    node |= ryml::MAP;
    node["FOV"] << camera.system->Fov;
    node["NearClip"] << camera.system->NearClip;
    node["FarClip"] << camera.system->FarClip;
  }

  // Physics
  if (entity.has_component<RigidbodyComponent>()) {
    auto node = entityNode["RigidbodyComponent"];
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
    auto node = entityNode["BoxColliderComponent"];
    node |= ryml::MAP;

    const auto& bc = entity.get_component<BoxColliderComponent>();
    node["Size"] << bc.size;
    node["Offset"] << bc.offset;
    node["Density"] << bc.density;
    node["Friction"] << bc.friction;
    node["Restitution"] << bc.restitution;
  }

  if (entity.has_component<SphereColliderComponent>()) {
    auto node = entityNode["SphereColliderComponent"];
    node |= ryml::MAP;

    const auto& sc = entity.get_component<SphereColliderComponent>();
    node["Radius"] << sc.radius;
    node["Offset"] << sc.offset;
    node["Density"] << sc.density;
    node["Friction"] << sc.friction;
    node["Restitution"] << sc.restitution;
  }

  if (entity.has_component<CapsuleColliderComponent>()) {
    auto node = entityNode["CapsuleColliderComponent"];
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
    auto node = entityNode["TaperedCapsuleColliderComponent"];
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
    auto node = entityNode["CylinderColliderComponent"];
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
    auto node = entityNode["CharacterControllerComponent"];
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

  if (entity.has_component<CustomComponent>()) {
    auto node = entityNode["CustomComponent"];
    node |= ryml::MAP;

    const auto& component = entity.get_component<CustomComponent>();

    node["Name"] << component.name;
    auto fieldsNode = node["Fields"];
    fieldsNode |= ryml::MAP;
    for (const auto& field : component.fields) {
      auto fieldNode = fieldsNode[field.name.c_str()];
      fieldNode |= ryml::MAP;
      fieldNode["Type"] << (int)field.type;
      fieldNode["Value"] << field.value;
    }
  }
}

UUID EntitySerializer::DeserializeEntity(ryml::ConstNodeRef entityNode, Scene* scene, bool preserveUUID) {
  const auto st = std::string(entityNode["Entity"].val().data());
  const uint64_t uuid = std::stoull(st.substr(0, st.find('\n')));

  std::string name;
  if (entityNode.has_child("TagComponent"))
    entityNode["TagComponent"]["Tag"] >> name;

  Entity deserializedEntity;
  if (preserveUUID)
    deserializedEntity = scene->create_entity_with_uuid(uuid, name);
  else
    deserializedEntity = scene->create_entity(name);

  if (entityNode.has_child("TransformComponent")) {
    auto& tc = deserializedEntity.get_component<TransformComponent>();
    const auto& node = entityNode["TransformComponent"];

    glm::read(node["Translation"], &tc.translation);
    glm::read(node["Rotation"], &tc.rotation);
    glm::read(node["Scale"], &tc.scale);
  }

  if (entityNode.has_child("RelationshipComponent")) {
    auto& rc = deserializedEntity.get_component<RelationshipComponent>();
    const auto node = entityNode["RelationshipComponent"];
    uint64_t parentID = 0;
    node["Parent"] >> parentID;
    rc.parent = parentID;

    size_t childCount = 0;
    node["ChildCount"] >> childCount;
    rc.children.clear();
    rc.children.reserve(childCount);
    const auto children = node["Children"];

    if (children.num_children() == childCount) {
      for (size_t i = 0; i < childCount; i++) {
        uint64_t childID = 0;
        children[i] >> childID;
        UUID child = childID;
        if (child)
          rc.children.push_back(child);
      }
    }
  }

  if (entityNode.has_child("MeshRendererComponent")) {
    const auto& node = entityNode["MeshRendererComponent"];

    std::string meshPath;
    uint32_t submeshIndex = 0;
    node["Mesh"] >> meshPath;
    node["SubmeshIndex"] >> submeshIndex;

    deserializedEntity.add_component<MeshRendererComponent>(AssetManager::get_mesh_asset(meshPath)).submesh_index =
      submeshIndex;
  }

  if (entityNode.has_child("MaterialComponent")) {
    auto& mc = deserializedEntity.add_component_internal<MaterialComponent>();

    const auto& node = entityNode["MaterialComponent"];

    std::string assetPath;
    if (node.has_child("Path"))
      node["Path"] >> assetPath;

    if (!assetPath.empty()) {
      mc.materials.clear();
      auto mat = mc.materials.emplace_back(create_ref<Material>());
      MaterialSerializer serializer(mat);
      serializer.Deserialize(assetPath);
      mc.using_material_asset = true;
    }
  }

  if (entityNode.has_child("LightComponent")) {
    auto& light = deserializedEntity.add_component_internal<LightComponent>();
    const auto& node = entityNode["LightComponent"];

    SetEnum(node["Type"], light.type);
    node["UseColorTemperatureMode"] >> light.use_color_temperature_mode;
    node["Temperature"] >> light.temperature;
    node["Intensity"] >> light.intensity;
    glm::read(node["Color"], &light.color);
    node["Range"] >> light.range;
    node["CutOffAngle"] >> light.cut_off_angle;
    node["OuterCutOffAngle"] >> light.outer_cut_off_angle;
    SetEnum(node["ShadowQuality"], light.shadow_quality);
  }

  if (entityNode.has_child("SkyLightComponent")) {
    auto& light = deserializedEntity.add_component_internal<SkyLightComponent>();
    const auto& node = entityNode["SkyLightComponent"];

    std::string path{};
    node["ImagePath"] >> path;
#if 0 //TODO:
    if (!path.empty()) {
      ImageCreateInfo CubeMapDesc;
      CubeMapDesc.Path = FileUtils::GetPreferredPath(path);
      CubeMapDesc.Type = ImageType::TYPE_CUBE;
      light.Cubemap = AssetManager::GetImageAsset(CubeMapDesc).Data;
      scene->GetRenderer().Dispatcher.trigger(SceneRenderer::SkyboxLoadEvent{light.Cubemap});
    }
#endif
  }

  if (entityNode.has_child("PostProcessProbe")) {
    auto& probe = deserializedEntity.add_component_internal<PostProcessProbe>();
    const auto& node = entityNode["PostProcessProbe"];

    node["VignetteEnabled"] >> probe.vignette_enabled;
    node["VignetteIntensity"] >> probe.vignette_intensity;

    node["FilmGrainEnabled"] >> probe.film_grain_enabled;
    node["FilmGrainIntensity"] >> probe.film_grain_intensity;

    node["ChromaticAberrationEnabled"] >> probe.chromatic_aberration_enabled;
    node["ChromaticAberrationIntensity"] >> probe.chromatic_aberration_intensity;

    node["SharpenEnabled"] >> probe.sharpen_enabled;
    node["SharpenIntensity"] >> probe.sharpen_intensity;
  }

  if (entityNode.has_child("CameraComponent")) {
    auto& camera = deserializedEntity.add_component_internal<CameraComponent>();
    const auto& node = entityNode["CameraComponent"];

    float fov = 0;
    float nearclip = 0;
    float farclip = 0;
    node["FOV"] >> fov;
    node["NearClip"] >> nearclip;
    node["FarClip"] >> farclip;

    camera.system->SetFov(fov);
    camera.system->SetNear(nearclip);
    camera.system->SetFar(farclip);
  }

  if (entityNode.has_child("RigidbodyComponent")) {
    auto& rb = deserializedEntity.add_component_internal<RigidbodyComponent>();
    const auto node = entityNode["RigidbodyComponent"];

    SetEnum(node["Type"], rb.type);
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

  if (entityNode.has_child("BoxColliderComponent")) {
    const auto node = entityNode["BoxColliderComponent"];
    auto& bc = deserializedEntity.add_component_internal<BoxColliderComponent>();

    node["Size"] >> bc.size;
    glm::read(node["Offset"], &bc.offset);
    node["Density"] >> bc.density;
    node["Friction"] >> bc.friction;
    node["Restitution"] >> bc.restitution;
  }

  if (entityNode.has_child("SphereColliderComponent")) {
    auto node = entityNode["SphereColliderComponent"];

    auto& sc = deserializedEntity.add_component_internal<SphereColliderComponent>();
    node["Radius"] >> sc.radius;
    glm::read(node["Offset"], &sc.offset);
    node["Offset"] >> sc.offset;
    node["Density"] >> sc.density;
    node["Friction"] >> sc.friction;
    node["Restitution"] >> sc.restitution;
  }

  if (entityNode.has_child("CapsuleColliderComponent")) {
    auto node = entityNode["CapsuleColliderComponent"];

    auto& cc = deserializedEntity.add_component_internal<CapsuleColliderComponent>();
    node["Height"] >> cc.height;
    node["Radius"] >> cc.radius;
    glm::read(node["Offset"], &cc.offset);
    node["Density"] >> cc.density;
    node["Friction"] >> cc.friction;
    node["Restitution"] >> cc.restitution;
  }

  if (entityNode.has_child("TaperedCapsuleColliderComponent")) {
    const auto node = entityNode["TaperedCapsuleColliderComponent"];

    auto& tcc = deserializedEntity.add_component_internal<TaperedCapsuleColliderComponent>();
    node["Height"] >> tcc.height;
    node["TopRadius"] >> tcc.top_radius;
    node["BottomRadius"] >> tcc.bottom_radius;
    glm::read(node["Offset"], &tcc.offset);
    node["Density"] >> tcc.density;
    node["Friction"] >> tcc.friction;
    node["Restitution"] >> tcc.restitution;
  }

  if (entityNode.has_child("CylinderColliderComponent")) {
    auto node = entityNode["CylinderColliderComponent"];

    auto& cc = deserializedEntity.add_component_internal<CapsuleColliderComponent>();
    node["Height"] >> cc.height;
    node["Radius"] >> cc.radius;
    glm::read(node["Offset"], &cc.offset);
    node["Density"] >> cc.density;
    node["Friction"] >> cc.friction;
    node["Restitution"] >> cc.restitution;
  }

  if (entityNode.has_child("CharacterControllerComponent")) {
    auto& component = deserializedEntity.add_component_internal<CharacterControllerComponent>();
    const auto& node = entityNode["CharacterControllerComponent"];

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

  if (entityNode.has_child("CustomComponent")) {
    auto& component = deserializedEntity.add_component_internal<CustomComponent>();
    const auto& node = entityNode["CustomComponent"];

    node["Name"] >> component.name;
    auto fieldsNode = node["Fields"];
    for (size_t i = 0; i < fieldsNode.num_children(); i++) {
      CustomComponent::ComponentField field;
      auto key = std::string(fieldsNode[i].get()->m_key.scalar.data());
      field.name = key.substr(0, key.find(':'));
      SetEnum(fieldsNode[i]["Type"], field.type);
      fieldsNode[i]["Value"] >> field.value;
      component.fields.emplace_back(field);
    }
  }

  return deserializedEntity.get_uuid();
}

void EntitySerializer::SerializeEntityAsPrefab(const char* filepath, Entity entity) {
  if (entity.has_component<PrefabComponent>()) {
    OX_CORE_ERROR("Entity already has a prefab component!");
    return;
  }

  ryml::Tree tree;

  ryml::NodeRef nodeRoot = tree.rootref();
  nodeRoot |= ryml::MAP;

  nodeRoot["Prefab"] << entity.add_component_internal<PrefabComponent>().id;

  ryml::NodeRef entitiesNode = nodeRoot["Entities"];
  entitiesNode |= ryml::SEQ;

  std::vector<Entity> entities;
  entities.push_back(entity);
  Entity::get_all_children(entity, entities);

  for (const auto& child : entities) {
    if (child)
      SerializeEntity(entity.get_scene(), entitiesNode, child);
  }

  std::stringstream ss;
  ss << tree;
  std::ofstream filestream(filepath);
  filestream << ss.str();
}

Entity EntitySerializer::DeserializeEntityAsPrefab(const char* filepath, Scene* scene) {
  auto content = FileUtils::read_file(filepath);
  if (content.empty()) {
    OX_CORE_ERROR(fmt::format("Couldn't read prefab file: {0}", filepath).c_str());

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
    OX_CORE_ERROR("Couldn't parse the prefab file {0}", FileSystem::GetFileName(filepath));
  }

  const ryml::ConstNodeRef root = tree.rootref();

  if (!root.has_child("Prefab")) {
    OX_CORE_ERROR("Prefab file doesn't contain a prefab{0}", FileSystem::GetFileName(filepath));
    return {};
  }

  const UUID prefabID = (uint64_t)root["Prefab"].val().data();

  if (!prefabID) {
    OX_CORE_ERROR("Invalid prefab ID {0}", FileSystem::GetFileName(filepath));
    return {};
  }

  if (root.has_child("Entities")) {
    const ryml::ConstNodeRef entitiesNode = root["Entities"];

    Entity rootEntity = {};
    std::unordered_map<UUID, UUID> oldNewIdMap;
    for (const auto& entity : entitiesNode) {
      uint64_t oldUUID;
      entity["Entity"] >> oldUUID;
      UUID newUUID = DeserializeEntity(entity, scene, false);
      oldNewIdMap.emplace(oldUUID, newUUID);

      if (!rootEntity)
        rootEntity = scene->get_entity_by_uuid(newUUID);
    }

    rootEntity.add_component_internal<PrefabComponent>().id = prefabID;

    // Fix parent/children UUIDs
    for (const auto& [_, newId] : oldNewIdMap) {
      auto& relationshipComponent = scene->get_entity_by_uuid(newId).get_relationship();
      UUID parent = relationshipComponent.parent;
      if (parent)
        relationshipComponent.parent = oldNewIdMap.at(parent);

      auto& children = relationshipComponent.children;
      for (auto& id : children)
        id = oldNewIdMap.at(id);
    }

    return rootEntity;
  }

  OX_CORE_ERROR("There are not entities in the prefab to deserialize! {0}", FileSystem::GetFileName(filepath));
  return {};
}
}
