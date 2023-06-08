#include "src/oxpch.h"
#include "EntitySerializer.h"

#include <fstream>
#include <ranges>

#include "Assets/AssetManager.h"

#include "Core/Entity.h"
#include "Core/YamlHelpers.h"

#include "Utils/FileUtils.h"
#include "Utils/StringUtils.h"

namespace Oxylus {
  template <typename T>
  void SetEnum(const ryml::ConstNodeRef& node, T& data) {
    int type = 0;
    node >> type;
    data = (T)type;
  }

  void EntitySerializer::SerializeEntity(Scene* scene, ryml::NodeRef& entities, Entity entity) {
    ryml::NodeRef entityNode = entities.append_child({ryml::MAP});

    if (entity.HasComponent<TagComponent>()) {
      const auto& tag = entity.GetComponent<TagComponent>();

      entityNode["Entity"] << entity.GetUUID();
      auto node = entityNode["TagComponent"];
      node |= ryml::MAP;
      node["Tag"] << tag.Tag;
    }

    if (entity.HasComponent<RelationshipComponent>()) {
      const auto& [Parent, Children] = entity.GetComponent<RelationshipComponent>();

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

    if (entity.HasComponent<TransformComponent>()) {
      const auto& tc = entity.GetComponent<TransformComponent>();
      auto node = entityNode["TransformComponent"];
      node |= ryml::MAP;
      auto translation = node["Translation"];
      auto rotation = node["Rotation"];
      auto scale = node["Scale"];
      glm::write(&translation, tc.Translation);
      glm::write(&rotation, tc.Rotation);
      glm::write(&scale, tc.Scale);
    }

    if (entity.HasComponent<MeshRendererComponent>()) {
      const auto& mrc = entity.GetComponent<MeshRendererComponent>();
      auto node = entityNode["MeshRendererComponent"];
      node |= ryml::MAP;
      node["Mesh"] << mrc.MeshGeometry->Path;
      node["SubmeshIndex"] << mrc.SubmesIndex;
    }

    if (entity.HasComponent<MaterialComponent>()) {
      const auto& mc = entity.GetComponent<MaterialComponent>();
      auto node = entityNode["MaterialComponent"];
      node |= ryml::MAP;
      if (mc.UsingMaterialAsset)
        node["Path"] << mc.Materials[0]->Path;
    }

    if (entity.HasComponent<LightComponent>()) {
      const auto& light = entity.GetComponent<LightComponent>();
      auto node = entityNode["LightComponent"];
      node |= ryml::MAP;
      auto colorNode = node["Color"];
      node["Type"] << (int)light.Type;
      node["UseColorTemperatureMode"] << light.UseColorTemperatureMode;
      node["Temperature"] << light.Temperature;
      glm::write(&colorNode, light.Color);
      node["Intensity"] << light.Intensity;
      node["Range"] << light.Range;
      node["CutOffAngle"] << light.CutOffAngle;
      node["OuterCutOffAngle"] << light.OuterCutOffAngle;
      node["ShadowQuality"] << (int)light.ShadowQuality;
    }

    if (entity.HasComponent<SkyLightComponent>()) {
      const auto& light = entity.GetComponent<SkyLightComponent>();
      auto node = entityNode["SkyLightComponent"];
      node |= ryml::MAP;
      std::string path = light.Cubemap ? light.Cubemap->GetDesc().Path : "";
      node["ImagePath"] << path;
      node["CubemapLodBias"] << light.CubemapLodBias;
    }

    if (entity.HasComponent<PostProcessProbe>()) {
      const auto& probe = entity.GetComponent<PostProcessProbe>();
      auto node = entityNode["PostProcessProbe"];
      node |= ryml::MAP;
      node["VignetteEnabled"] << probe.VignetteEnabled;
      node["VignetteIntensity"] << probe.VignetteIntensity;

      node["FilmGrainEnabled"] << probe.FilmGrainEnabled;
      node["FilmGrainIntensity"] << probe.FilmGrainIntensity;

      node["ChromaticAberrationEnabled"] << probe.ChromaticAberrationEnabled;
      node["ChromaticAberrationIntensity"] << probe.ChromaticAberrationIntensity;

      node["SharpenEnabled"] << probe.SharpenEnabled;
      node["SharpenIntensity"] << probe.SharpenIntensity;
    }

    if (entity.HasComponent<CameraComponent>()) {
      const auto& camera = entity.GetComponent<CameraComponent>();
      auto node = entityNode["CameraComponent"];
      node |= ryml::MAP;
      node["FOV"] << camera.System->Fov;
      node["NearClip"] << camera.System->NearClip;
      node["FarClip"] << camera.System->FarClip;
    }

    // Physics
    if (entity.HasComponent<RigidbodyComponent>()) {
      auto node = entityNode["RigidbodyComponent"];
      node |= ryml::MAP;

      const auto& rb = entity.GetComponent<RigidbodyComponent>();
      node["Type"] << static_cast<int>(rb.Type);
      node["Mass"] << rb.Mass;
      node["LinearDrag"] << rb.LinearDrag;
      node["AngularDrag"] << rb.AngularDrag;
      node["GravityScale"] << rb.GravityScale;
      node["AllowSleep"] << rb.AllowSleep;
      node["Awake"] << rb.Awake;
      node["Continuous"] << rb.Continuous;
      node["Interpolation"] << rb.Interpolation;
      node["IsSensor"] << rb.IsSensor;
    }

    if (entity.HasComponent<BoxColliderComponent>()) {
      auto node = entityNode["BoxColliderComponent"];
      node |= ryml::MAP;

      const auto& bc = entity.GetComponent<BoxColliderComponent>();
      node["Size"] << bc.Size;
      node["Offset"] << bc.Offset;
      node["Density"] << bc.Density;
      node["Friction"] << bc.Friction;
      node["Restitution"] << bc.Restitution;
    }

    if (entity.HasComponent<SphereColliderComponent>()) {
      auto node = entityNode["SphereColliderComponent"];
      node |= ryml::MAP;

      const auto& sc = entity.GetComponent<SphereColliderComponent>();
      node["Radius"] << sc.Radius;
      node["Offset"] << sc.Offset;
      node["Density"] << sc.Density;
      node["Friction"] << sc.Friction;
      node["Restitution"] << sc.Restitution;
    }

    if (entity.HasComponent<CapsuleColliderComponent>()) {
      auto node = entityNode["CapsuleColliderComponent"];
      node |= ryml::MAP;

      const auto& cc = entity.GetComponent<CapsuleColliderComponent>();
      node["Height"] << cc.Height;
      node["Radius"] << cc.Radius;
      node["Offset"] << cc.Offset;
      node["Density"] << cc.Density;
      node["Friction"] << cc.Friction;
      node["Restitution"] << cc.Restitution;
    }

    if (entity.HasComponent<TaperedCapsuleColliderComponent>()) {
      auto node = entityNode["TaperedCapsuleColliderComponent"];
      node |= ryml::MAP;

      const auto& tcc = entity.GetComponent<TaperedCapsuleColliderComponent>();
      node["Height"] << tcc.Height;
      node["TopRadius"] << tcc.TopRadius;
      node["BottomRadius"] << tcc.BottomRadius;
      node["Offset"] << tcc.Offset;
      node["Density"] << tcc.Density;
      node["Friction"] << tcc.Friction;
      node["Restitution"] << tcc.Restitution;
    }

    if (entity.HasComponent<CylinderColliderComponent>()) {
      auto node = entityNode["CylinderColliderComponent"];
      node |= ryml::MAP;

      const auto& cc = entity.GetComponent<CapsuleColliderComponent>();
      node["Height"] << cc.Height;
      node["Radius"] << cc.Radius;
      node["Offset"] << cc.Offset;
      node["Density"] << cc.Density;
      node["Friction"] << cc.Friction;
      node["Restitution"] << cc.Restitution;
    }

    if (entity.HasComponent<CharacterControllerComponent>()) {
      auto node = entityNode["CharacterControllerComponent"];
      node |= ryml::MAP;

      const auto& component = entity.GetComponent<CharacterControllerComponent>();

      // Size
      node["CharacterHeightStanding"] << component.CharacterHeightStanding;
      node["CharacterRadiusStanding"] << component.CharacterRadiusStanding;
      node["CharacterRadiusCrouching"] << component.CharacterRadiusCrouching;
      node["CharacterHeightCrouching"] << component.CharacterHeightCrouching;

      // Movement
      node["CharacterSpeed"] << component.CharacterSpeed;
      node["ControlMovementDuringJump"] << component.ControlMovementDuringJump;
      node["JumpSpeed"] << component.JumpSpeed;

      node["Friction"] << component.Friction;
      node["CollisionTolerance"] << component.CollisionTolerance;
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
      deserializedEntity = scene->CreateEntityWithUUID(uuid, name);
    else
      deserializedEntity = scene->CreateEntity(name);

    if (entityNode.has_child("TransformComponent")) {
      auto& tc = deserializedEntity.GetComponent<TransformComponent>();
      const auto& node = entityNode["TransformComponent"];

      glm::read(node["Translation"], &tc.Translation);
      glm::read(node["Rotation"], &tc.Rotation);
      glm::read(node["Scale"], &tc.Scale);
    }

    if (entityNode.has_child("RelationshipComponent")) {
      auto& rc = deserializedEntity.GetComponent<RelationshipComponent>();
      const auto node = entityNode["RelationshipComponent"];
      uint64_t parentID = 0;
      node["Parent"] >> parentID;
      rc.Parent = parentID;

      size_t childCount = 0;
      node["ChildCount"] >> childCount;
      rc.Children.clear();
      rc.Children.reserve(childCount);
      const auto children = node["Children"];

      if (children.num_children() == childCount) {
        for (size_t i = 0; i < childCount; i++) {
          uint64_t childID = 0;
          children[i] >> childID;
          UUID child = childID;
          if (child)
            rc.Children.push_back(child);
        }
      }
    }

    if (entityNode.has_child("MeshRendererComponent")) {
      const auto& node = entityNode["MeshRendererComponent"];

      std::string meshPath;
      uint32_t submeshIndex = 0;
      node["Mesh"] >> meshPath;
      node["SubmeshIndex"] >> submeshIndex;

      deserializedEntity.AddComponent<MeshRendererComponent>(AssetManager::GetMeshAsset(meshPath).Data).SubmesIndex = submeshIndex;
    }

    if (entityNode.has_child("MaterialComponent")) {
      auto& mc = deserializedEntity.AddComponentI<MaterialComponent>();

      const auto& node = entityNode["MaterialComponent"];

      std::string assetPath;
      if (node.has_child("Path"))
        node["Path"] >> assetPath;

      if (!assetPath.empty()) {
        mc.Materials.clear();
        mc.Materials.emplace_back(AssetManager::GetMaterialAsset(assetPath).Data);
        mc.UsingMaterialAsset = true;
      }
    }

    if (entityNode.has_child("LightComponent")) {
      auto& light = deserializedEntity.AddComponentI<LightComponent>();
      const auto& node = entityNode["LightComponent"];

      SetEnum(node["Type"], light.Type);
      node["UseColorTemperatureMode"] >> light.UseColorTemperatureMode;
      node["Temperature"] >> light.Temperature;
      node["Intensity"] >> light.Intensity;
      glm::read(node["Color"], &light.Color);
      node["Range"] >> light.Range;
      node["CutOffAngle"] >> light.CutOffAngle;
      node["OuterCutOffAngle"] >> light.OuterCutOffAngle;
      SetEnum(node["ShadowQuality"], light.ShadowQuality);
    }

    if (entityNode.has_child("SkyLightComponent")) {
      auto& light = deserializedEntity.AddComponentI<SkyLightComponent>();
      const auto& node = entityNode["SkyLightComponent"];

      std::string path{};
      node["ImagePath"] >> path;

      if (!path.empty()) {
        VulkanImageDescription CubeMapDesc;
        CubeMapDesc.Path = path;
        CubeMapDesc.Type = ImageType::TYPE_CUBE;
        light.Cubemap = AssetManager::GetImageAsset(CubeMapDesc).Data;
        scene->GetRenderer().Dispatcher.trigger(SceneRenderer::SkyboxLoadEvent{light.Cubemap});
      }

      node["CubemapLodBias"] >> light.CubemapLodBias;
    }

    if (entityNode.has_child("PostProcessProbe")) {
      auto& probe = deserializedEntity.AddComponentI<PostProcessProbe>();
      const auto& node = entityNode["PostProcessProbe"];

      node["VignetteEnabled"] >> probe.VignetteEnabled;
      node["VignetteIntensity"] >> probe.VignetteIntensity;

      node["FilmGrainEnabled"] >> probe.FilmGrainEnabled;
      node["FilmGrainIntensity"] >> probe.FilmGrainIntensity;

      node["ChromaticAberrationEnabled"] >> probe.ChromaticAberrationEnabled;
      node["ChromaticAberrationIntensity"] >> probe.ChromaticAberrationIntensity;

      node["SharpenEnabled"] >> probe.SharpenEnabled;
      node["SharpenIntensity"] >> probe.SharpenIntensity;
    }

    if (entityNode.has_child("CameraComponent")) {
      auto& camera = deserializedEntity.AddComponentI<CameraComponent>();
      const auto& node = entityNode["CameraComponent"];

      float fov = 0;
      float nearclip = 0;
      float farclip = 0;
      node["FOV"] >> fov;
      node["NearClip"] >> nearclip;
      node["FarClip"] >> farclip;

      camera.System->SetFov(fov);
      camera.System->SetNear(nearclip);
      camera.System->SetFar(farclip);
    }

    if (entityNode.has_child("RigidbodyComponent")) {
      auto& rb = deserializedEntity.AddComponentI<RigidbodyComponent>();
      const auto node = entityNode["RigidbodyComponent"];

      SetEnum(node["Type"], rb.Type);
      node["Mass"] >> rb.Mass;
      node["LinearDrag"] >> rb.LinearDrag;
      node["AngularDrag"] >> rb.AngularDrag;
      node["GravityScale"] >> rb.GravityScale;
      node["AllowSleep"] >> rb.AllowSleep;
      node["Awake"] >> rb.Awake;
      node["Continuous"] >> rb.Continuous;
      node["Interpolation"] >> rb.Interpolation;
      node["IsSensor"] >> rb.IsSensor;
    }

    if (entityNode.has_child("BoxColliderComponent")) {
      const auto node = entityNode["BoxColliderComponent"];
      auto& bc = deserializedEntity.AddComponentI<BoxColliderComponent>();

      node["Size"] >> bc.Size;
      glm::read(node["Offset"], &bc.Offset);
      node["Density"] >> bc.Density;
      node["Friction"] >> bc.Friction;
      node["Restitution"] >> bc.Restitution;
    }

    if (entityNode.has_child("SphereColliderComponent")) {
      auto node = entityNode["SphereColliderComponent"];

      auto& sc = deserializedEntity.AddComponentI<SphereColliderComponent>();
      node["Radius"] >> sc.Radius;
      glm::read(node["Offset"], &sc.Offset);
      node["Offset"] >> sc.Offset;
      node["Density"] >> sc.Density;
      node["Friction"] >> sc.Friction;
      node["Restitution"] >> sc.Restitution;
    }

    if (entityNode.has_child("CapsuleColliderComponent")) {
      auto node = entityNode["CapsuleColliderComponent"];

      auto& cc = deserializedEntity.AddComponentI<CapsuleColliderComponent>();
      node["Height"] >> cc.Height;
      node["Radius"] >> cc.Radius;
      glm::read(node["Offset"], &cc.Offset);
      node["Density"] >> cc.Density;
      node["Friction"] >> cc.Friction;
      node["Restitution"] >> cc.Restitution;
    }

    if (entityNode.has_child("TaperedCapsuleColliderComponent")) {
      const auto node = entityNode["TaperedCapsuleColliderComponent"];

      auto& tcc = deserializedEntity.AddComponentI<TaperedCapsuleColliderComponent>();
      node["Height"] >> tcc.Height;
      node["TopRadius"] >> tcc.TopRadius;
      node["BottomRadius"] >> tcc.BottomRadius;
      glm::read(node["Offset"], &tcc.Offset);
      node["Density"] >> tcc.Density;
      node["Friction"] >> tcc.Friction;
      node["Restitution"] >> tcc.Restitution;
    }

    if (entityNode.has_child("CylinderColliderComponent")) {
      auto node = entityNode["CylinderColliderComponent"];

      auto& cc = deserializedEntity.AddComponentI<CapsuleColliderComponent>();
      node["Height"] >> cc.Height;
      node["Radius"] >> cc.Radius;
      glm::read(node["Offset"], &cc.Offset);
      node["Density"] >> cc.Density;
      node["Friction"] >> cc.Friction;
      node["Restitution"] >> cc.Restitution;
    }

    if (entityNode.has_child("CharacterControllerComponent")) {
      auto& component = deserializedEntity.AddComponentI<CharacterControllerComponent>();
      const auto& node = entityNode["CharacterControllerComponent"];

      // Size
      node["CharacterHeightStanding"] >> component.CharacterHeightStanding;
      node["CharacterRadiusStanding"] >> component.CharacterRadiusStanding;
      node["CharacterRadiusCrouching"] >> component.CharacterRadiusCrouching;
      node["CharacterHeightCrouching"] >> component.CharacterHeightCrouching;

      // Movement
      node["CharacterSpeed"] >> component.CharacterSpeed;
      node["ControlMovementDuringJump"] >> component.ControlMovementDuringJump;
      node["JumpSpeed"] >> component.JumpSpeed;

      node["Friction"] >> component.Friction;
      node["CollisionTolerance"] >> component.CollisionTolerance;
    }

    return deserializedEntity.GetUUID();
  }

  void EntitySerializer::SerializeEntityAsPrefab(const char* filepath, Entity entity) {
    if (entity.HasComponent<PrefabComponent>()) {
      OX_CORE_ERROR("Entity already has a prefab component!");
      return;
    }

    ryml::Tree tree;

    ryml::NodeRef nodeRoot = tree.rootref();
    nodeRoot |= ryml::MAP;

    nodeRoot["Prefab"] << entity.AddComponentI<PrefabComponent>().ID;

    ryml::NodeRef entitiesNode = nodeRoot["Entities"];
    entitiesNode |= ryml::SEQ;

    std::vector<Entity> entities;
    entities.push_back(entity);
    Entity::GetAllChildren(entity, entities);

    for (const auto& child : entities) {
      if (child)
        SerializeEntity(entity.GetScene(), entitiesNode, child);
    }

    std::stringstream ss;
    ss << tree;
    std::ofstream filestream(filepath);
    filestream << ss.str();
  }

  Entity EntitySerializer::DeserializeEntityAsPrefab(const char* filepath, Scene* scene) {
    auto content = FileUtils::ReadFile(filepath);
    if (!content) {
      OX_CORE_ASSERT(content, fmt::format("Couldn't read prefab file: {0}", filepath).c_str());

      // Try to read it again from assets path
      content = FileUtils::ReadFile(AssetManager::GetAssetFileSystemPath(filepath).string());
      if (content)
        OX_CORE_INFO("Could load the material file from assets path: {0}", filepath);
      else {
        return {};
      }
    }


    const ryml::Tree tree = ryml::parse_in_arena(ryml::to_csubstr(content.value()));

    if (tree.empty()) {
      OX_CORE_BERROR("Couldn't parse the prefab file {0}", StringUtils::GetName(filepath));
    }

    const ryml::ConstNodeRef root = tree.rootref();

    if (!root.has_child("Prefab")) {
      OX_CORE_BERROR("Prefab file doesn't contain a prefab{0}", StringUtils::GetName(filepath));
      return {};
    }

    const UUID prefabID = (uint64_t)root["Prefab"].val().data();

    if (!prefabID) {
      OX_CORE_ERROR("Invalid prefab ID {0}", StringUtils::GetName(filepath));
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
          rootEntity = scene->GetEntityByUUID(newUUID);
      }

      rootEntity.AddComponentI<PrefabComponent>().ID = prefabID;

      // Fix parent/children UUIDs
      for (const auto& [_, newId] : oldNewIdMap) {
        auto& relationshipComponent = scene->GetEntityByUUID(newId).GetRelationship();
        UUID parent = relationshipComponent.Parent;
        if (parent)
          relationshipComponent.Parent = oldNewIdMap.at(parent);

        auto& children = relationshipComponent.Children;
        for (auto& id : children)
          id = oldNewIdMap.at(id);
      }

      return rootEntity;
    }

    OX_CORE_BERROR("There are not entities in the prefab to deserialize! {0}", StringUtils::GetName(filepath));
    return {};
  }
}
