#include "src/oxpch.h"
#include "Scene.h"

#include "Core/Entity.h"
#include "Render/Camera.h"
#include "Render/Vulkan/VulkanRenderer.h"
#include "Utils/Profiler.h"
#include "Utils/TimeStep.h"

#include <glm/glm.hpp>

#include <unordered_map>
#include <glm/gtc/type_ptr.hpp>

namespace Oxylus {
  Scene::Scene() {
    Init();
    InitPhysics();
  }

  Scene::Scene(std::string name) : SceneName(std::move(name)) { }

  Scene::~Scene() = default;

  Scene::Scene(const Scene& scene) {
    const auto& reg = scene.m_Registry;
    this->SceneName = scene.SceneName;
    this->m_Registry.assign(reg.data(), reg.data() + reg.size(), reg.released());
  }

  void Scene::Init() {
    // Renderer
    m_SceneRenderer.Init(this);

    // Systems
    for (const auto& system : m_Systems) {
      system->OnInit();
    }
  }

  void Scene::InitPhysics() {
    // Physics
    m_JobSystem = CreateRef<JPH::JobSystemThreadPool>();
    m_JobSystem->Init(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);
    m_PhysicsSystem = CreateRef<JPH::PhysicsSystem>();
    m_PhysicsSystem->Init(
      Physics::MAX_BODIES,
      0,
      Physics::MAX_BODY_PAIRS,
      Physics::MAX_CONTACT_CONSTRAINS,
      Physics::s_LayerInterface,
      Physics::s_ObjectVsBroadPhaseLayerFilterInterface,
      Physics::s_ObjectLayerPairFilterInterface);
    m_BodyInterface = &m_PhysicsSystem->GetBodyInterface();
  }

  Entity Scene::CreateEntity(const std::string& name) {
    return CreateEntityWithUUID(UUID(), name);
  }

  Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name) {
    Entity entity = {m_Registry.create(), this};
    m_EntityMap.emplace(uuid, entity);
    entity.AddComponentI<IDComponent>(uuid);
    entity.AddComponentI<RelationshipComponent>();
    entity.AddComponentI<TransformComponent>();
    entity.AddComponentI<TagComponent>().Tag = name.empty() ? "Entity" : name;
    return entity;
  }

  void Scene::UpdatePhysics(bool simulating) {
    if (!simulating) {
      const auto group = m_Registry.view<TransformComponent, RigidBodyComponent>();
      for (const auto entity : group) {
        auto [transform, rb] = group.get<TransformComponent, RigidBodyComponent>(entity);
        m_BodyInterface->SetPosition(rb.BodyID, {transform.Translation.x, transform.Translation.y, transform.Translation.z}, JPH::EActivation::DontActivate);
        auto quat = glm::quat(transform.Rotation);
        m_BodyInterface->SetRotation(rb.BodyID, {quat.x, quat.y, quat.z, quat.w}, JPH::EActivation::DontActivate);
      }
      return;
    }

    constexpr float cDeltaTime = 1.0f / 60.0f; // Update rate
    constexpr int cCollisionSteps = 1;
    constexpr int cIntegrationSubSteps = 1;

    m_PhysicsSystem->Update(cDeltaTime, cCollisionSteps, cIntegrationSubSteps, Physics::s_TempAllocator, m_JobSystem.get());

    // Physics
    {
      ZoneScopedN("Physics System");
      // Rigidbody
      {
        const auto group = m_Registry.view<TransformComponent, RigidBodyComponent>();
        for (const auto entity : group) {
          auto [transform, rb] = group.get<TransformComponent, RigidBodyComponent>(entity);
          transform.Translation = glm::make_vec3(m_BodyInterface->GetPosition(rb.BodyID).mF32);
          transform.Rotation = glm::eulerAngles(glm::make_quat(m_BodyInterface->GetRotation(rb.BodyID).GetXYZW().mF32));
        }
      }
    }
  }

  void Scene::IterateOverMeshNode(const Ref<Mesh>& mesh, const std::vector<Mesh::Node*>& node, Entity parent) {
    for (const auto child : node) {
      Entity entity = CreateEntity(child->Name).SetParent(parent);
      if (child->ContainsMesh) {
        entity.AddComponentI<MeshRendererComponent>(mesh).SubmesIndex = child->MeshIndex;
        entity.GetComponent<MaterialComponent>().Materials = mesh->GetMaterialsAsRef();
      }
      IterateOverMeshNode(mesh, child->Children, entity);
    }
  }

  void Scene::CreateEntityWithMesh(const Asset<Mesh>& meshAsset) {
    const auto& mesh = meshAsset.Data;
    for (const auto& node : mesh->Nodes) {
      Entity entity = CreateEntity(node->Name);
      if (node->ContainsMesh) {
        entity.AddComponentI<MeshRendererComponent>(mesh).SubmesIndex = node->MeshIndex;
        entity.GetComponent<MaterialComponent>().Materials = mesh->GetMaterialsAsRef();
      }
      IterateOverMeshNode(mesh, node->Children, entity);
    }
  }

  void Scene::DestroyEntity(const Entity entity) {
    entity.Deparent();
    const auto children = entity.GetComponent<RelationshipComponent>().Children;

    for (size_t i = 0; i < children.size(); i++) {
      if (const Entity childEntity = GetEntityByUUID(children[i]))
        DestroyEntity(childEntity);
    }

    m_EntityMap.erase(entity.GetUUID());
    m_Registry.destroy(entity);
  }

  template <typename... Component>
  static void CopyComponent(entt::registry& dst,
                            entt::registry& src,
                            const std::unordered_map<UUID, entt::entity>& enttMap) {
    ([&] {
      auto view = src.view<Component>();
      for (auto srcEntity : view) {
        entt::entity dstEntity = enttMap.at(src.get<IDComponent>(srcEntity).ID);

        Component& srcComponent = src.get<Component>(srcEntity);
        dst.emplace_or_replace<Component>(dstEntity, srcComponent);
      }
    }(), ...);
  }

  template <typename... Component>
  static void CopyComponent(ComponentGroup<Component...>,
                            entt::registry& dst,
                            entt::registry& src,
                            const std::unordered_map<UUID, entt::entity>& enttMap) {
    CopyComponent<Component...>(dst, src, enttMap);
  }

  template <typename... Component>
  static void CopyComponentIfExists(Entity dst, Entity src) {
    ([&] {
      if (src.HasComponent<Component>())
        dst.AddOrReplaceComponent<Component>(src.GetComponent<Component>());
    }(), ...);
  }

  template <typename... Component>
  static void CopyComponentIfExists(ComponentGroup<Component...>,
                                    Entity dst,
                                    Entity src) {
    CopyComponentIfExists<Component...>(dst, src);
  }

  void Scene::DuplicateEntity(Entity entity) {
    ZoneScoped;
    const Entity newEntity = CreateEntity(entity.GetName());
    CopyComponentIfExists(AllComponents{}, newEntity, entity);
  }

  void Scene::OnPlay() {
    ZoneScoped;
  }

  void Scene::OnStop() { }

  Entity Scene::FindEntity(const std::string_view& name) {
    ZoneScoped;
    const auto group = m_Registry.view<TagComponent>();
    for (const auto& entity : group) {
      auto& tag = group.get<TagComponent>(entity);
      if (tag.Tag == name) {
        return Entity{entity, this};
      }
    }
    return {};
  }

  bool Scene::HasEntity(UUID uuid) const {
    ZoneScoped;
    return m_EntityMap.contains(uuid);
  }

  Entity Scene::GetEntityByUUID(UUID uuid) {
    ZoneScoped;
    const auto& it = m_EntityMap.find(uuid);
    if (it != m_EntityMap.end())
      return {it->second, this};

    return {};
  }

  Ref<Scene> Scene::Copy(const Ref<Scene>& other) {
    ZoneScoped;
    Ref<Scene> newScene = CreateRef<Scene>();

    auto& srcSceneRegistry = other->m_Registry;
    auto& dstSceneRegistry = newScene->m_Registry;

    // Create entities in new scene
    const auto view = srcSceneRegistry.view<IDComponent, TagComponent>();
    for (const auto e : view) {
      auto [id, tag] = view.get<IDComponent, TagComponent>(e);
      const auto& name = tag.Tag;
      Entity newEntity = newScene->CreateEntityWithUUID(id.ID, name);
      newEntity.GetComponent<TagComponent>().Enabled = tag.Enabled;
    }

    for (const auto e : view) {
      Entity src = {e, other.get()};
      Entity dst = newScene->GetEntityByUUID(view.get<IDComponent>(e).ID);
      if (Entity srcParent = src.GetParent())
        dst.SetParent(newScene->GetEntityByUUID(srcParent.GetUUID()));
    }

    // Copy physics
    newScene->m_PhysicsSystem = CreateRef<JPH::PhysicsSystem>();
    newScene->m_PhysicsSystem->Init(
      Physics::MAX_BODIES,
      0,
      Physics::MAX_BODY_PAIRS,
      Physics::MAX_CONTACT_CONSTRAINS,
      Physics::s_LayerInterface,
      Physics::s_ObjectVsBroadPhaseLayerFilterInterface,
      Physics::s_ObjectLayerPairFilterInterface);
    newScene->m_BodyInterface = &newScene->m_PhysicsSystem->GetBodyInterface();

    // Copy bodies
    JPH::BodyIDVector otherBodyIDs = {};
    other->m_PhysicsSystem->GetBodies(otherBodyIDs);
    std::vector<JPH::Body*> otherBodies = {};
    for (auto& id : otherBodyIDs) {
      otherBodies.emplace_back(other->m_PhysicsSystem->GetBodyLockInterface().TryGetBody(id));
    }

    // Add copied bodies in batch
    JPH::BodyIDVector newBodyIDs = {};
    for (const auto& b : otherBodies) {
      newBodyIDs.emplace_back(newScene->m_BodyInterface->CreateBody(b->GetBodyCreationSettings())->GetID());
    }
    const auto state = newScene->m_BodyInterface->AddBodiesPrepare(newBodyIDs.data(), (int)newBodyIDs.size());
    newScene->m_BodyInterface->AddBodiesFinalize(newBodyIDs.data(), (int)newBodyIDs.size(), state, JPH::EActivation::Activate);

    newScene->m_JobSystem = other->m_JobSystem;

    // Copy components (except IDComponent and TagComponent)
    CopyComponent(AllComponents{}, dstSceneRegistry, srcSceneRegistry, newScene->m_EntityMap);

    return newScene;
  }

  void Scene::RenderScene() const {
    m_SceneRenderer.Render();
  }

  void Scene::UpdateSystems(float deltaTime) {
    ZoneScopedN("Update Systems");
    for (const auto& system : m_Systems) {
      system->OnUpdate(this, deltaTime);
    }
  }

  void Scene::OnUpdate(float deltaTime) {
    ZoneScoped;

    // Camera
    {
      ZoneScopedN("Camera System");
      const auto group = m_Registry.view<TransformComponent, CameraComponent>();
      for (const auto entity : group) {
        auto [transform, camera] = group.get<TransformComponent, CameraComponent>(entity);
        camera.System->Update(transform.Translation, transform.Rotation);
        VulkanRenderer::SetCamera(*camera.System);
      }
    }

    UpdateSystems(deltaTime);
    RenderScene();
    UpdatePhysics();

    // Audio
    {
      const auto listenerView = m_Registry.group<AudioListenerComponent>(entt::get<TransformComponent>);
      for (auto&& [e, ac, tc] : listenerView.each()) {
        ac.Listener = CreateRef<AudioListener>();
        if (ac.Active) {
          const glm::mat4 inverted = glm::inverse(Entity(e, this).GetWorldTransform());
          const glm::vec3 forward = normalize(glm::vec3(inverted[2]));
          ac.Listener->SetConfig(ac.Config);
          ac.Listener->SetPosition(tc.Translation);
          ac.Listener->SetDirection(-forward);
          break;
        }
      }

      const auto sourceView = m_Registry.group<AudioSourceComponent>(entt::get<TransformComponent>);
      for (auto&& [e, ac, tc] : sourceView.each()) {
        if (ac.Source) {
          const glm::mat4 inverted = glm::inverse(Entity(e, this).GetWorldTransform());
          const glm::vec3 forward = normalize(glm::vec3(inverted[2]));
          ac.Source->SetConfig(ac.Config);
          ac.Source->SetPosition(tc.Translation);
          ac.Source->SetDirection(forward);
          if (ac.Config.PlayOnAwake)
            ac.Source->Play();
        }
      }
    }
  }

  void Scene::OnEditorUpdate([[maybe_unused]] float deltaTime, Camera& camera) {
    RenderScene();
    UpdatePhysics(false);

    VulkanRenderer::SetCamera(camera);
  }

  template <typename T>
  void Scene::OnComponentAdded(Entity entity, T& component) {
    static_assert(sizeof(T) == 0);
  }

  template <>
  void Scene::OnComponentAdded<TagComponent>(Entity entity, TagComponent& component) { }

  template <>
  void Scene::OnComponentAdded<IDComponent>(Entity entity, IDComponent& component) { }

  template <>
  void Scene::OnComponentAdded<RelationshipComponent>(Entity entity, RelationshipComponent& component) { }

  template <>
  void Scene::OnComponentAdded<PrefabComponent>(Entity entity, PrefabComponent& component) { }

  template <>
  void Scene::OnComponentAdded<TransformComponent>(Entity entity, TransformComponent& component) { }

  template <>
  void Scene::OnComponentAdded<CameraComponent>(Entity entity, CameraComponent& component) { }

  template <>
  void Scene::OnComponentAdded<AudioSourceComponent>(Entity entity, AudioSourceComponent& component) { }

  template <>
  void Scene::OnComponentAdded<AudioListenerComponent>(Entity entity, AudioListenerComponent& component) { }

  template <>
  void Scene::OnComponentAdded<MeshRendererComponent>(Entity entity, MeshRendererComponent& component) {
    entity.AddComponentI<MaterialComponent>();
  }

  template <>
  void Scene::OnComponentAdded<SkyLightComponent>(Entity entity, SkyLightComponent& component) { }

  template <>
  void Scene::OnComponentAdded<MaterialComponent>(Entity entity, MaterialComponent& component) {
    if (component.Materials.empty()) {
      if (entity.HasComponent<MeshRendererComponent>())
        component.Materials = entity.GetComponent<MeshRendererComponent>().MeshGeometry->GetMaterialsAsRef();
    }
  }

  template <>
  void Scene::OnComponentAdded<LightComponent>(Entity entity, LightComponent& component) { }

  template <>
  void Scene::OnComponentAdded<PostProcessProbe>(Entity entity, PostProcessProbe& component) { }

  template <>
  void Scene::OnComponentAdded<ParticleSystemComponent>(Entity entity,
                                                        ParticleSystemComponent& component) { }

  template <>
  void Scene::OnComponentAdded<BoxColliderComponent>(Entity entity, BoxColliderComponent& component) { }

  template <>
  void Scene::OnComponentAdded<MeshColliderComponent>(Entity entity, MeshColliderComponent& component) { }

  template <>
  void Scene::OnComponentAdded<RigidBodyComponent>(Entity entity, RigidBodyComponent& component) { }
}
