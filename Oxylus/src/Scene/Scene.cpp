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

#include "Jolt/Physics/Character/Character.h"
#include "Jolt/Physics/Collision/Shape/CapsuleShape.h"
#include "Jolt/Physics/Collision/Shape/CylinderShape.h"
#include "Jolt/Physics/Collision/Shape/MutableCompoundShape.h"
#include "Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h"
#include "Jolt/Physics/Collision/Shape/TaperedCapsuleShape.h"
#include "Physics/PhysicsMaterial.h"
#include "Physics/PhysicsUtils.h"

namespace Oxylus {
  Scene::Scene() {
    Init();
  }

  Scene::Scene(std::string name) : SceneName(std::move(name)) { }

  Scene::~Scene() {
    if (m_IsRunning)
      OnRuntimeStop();
  }

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

  void Scene::UpdatePhysics(Timestep deltaTime) {
    OX_SCOPED_ZONE;
    // Minimum stable value is 16.0
    constexpr float physicsStepRate = 64.0f;
    constexpr float physicsTs = 1.0f / physicsStepRate;

    bool stepped = false;
    m_PhysicsFrameAccumulator += deltaTime;

    while (m_PhysicsFrameAccumulator >= physicsTs) {
      Physics::Step(physicsTs);

      m_PhysicsFrameAccumulator -= physicsTs;
      stepped = true;
    }

    const float interpolationFactor = m_PhysicsFrameAccumulator / physicsTs;

    const auto& bodyInterface = Physics::GetPhysicsSystem()->GetBodyInterface();
    const auto view = m_Registry.group<RigidbodyComponent>(entt::get<TransformComponent>);
    for (auto&& [e, rb, tc] : view.each()) {
      if (!rb.RuntimeBody)
        continue;

      PhysicsUtils::DebugDraw(this, e);

      const auto* body = static_cast<const JPH::Body*>(rb.RuntimeBody);

      if (!bodyInterface.IsActive(body->GetID()))
        continue;

      if (rb.Interpolation) {
        if (stepped) {
          JPH::Vec3 position = body->GetPosition();
          JPH::Vec3 rotation = body->GetRotation().GetEulerAngles();

          rb.PreviousTranslation = rb.Translation;
          rb.PreviousRotation = rb.Rotation;
          rb.Translation = {position.GetX(), position.GetY(), position.GetZ()};
          rb.Rotation = glm::vec3(rotation.GetX(), rotation.GetY(), rotation.GetZ());
        }

        tc.Translation = glm::lerp(rb.PreviousTranslation, rb.Translation, interpolationFactor);
        tc.Rotation = glm::eulerAngles(glm::slerp(rb.PreviousRotation, rb.Rotation, interpolationFactor));
      }
      else {
        const JPH::Vec3 position = body->GetPosition();
        JPH::Vec3 rotation = body->GetRotation().GetEulerAngles();

        rb.PreviousTranslation = rb.Translation;
        rb.PreviousRotation = rb.Rotation;
        rb.Translation = {position.GetX(), position.GetY(), position.GetZ()};
        rb.Rotation = glm::vec3(rotation.GetX(), rotation.GetY(), rotation.GetZ());
        tc.Translation = rb.Translation;
        tc.Rotation = glm::eulerAngles(rb.Rotation);
      }
    }

    // Character
    {
      const auto chView = m_Registry.view<TransformComponent, CharacterControllerComponent>();
      for (auto&& [e, tc, ch] : chView.each()) {
        ch.Character->PostSimulation(ch.CollisionTolerance);
        if (ch.Interpolation) {
          if (stepped) {
            JPH::Vec3 position = ch.Character->GetPosition();
            JPH::Vec3 rotation = ch.Character->GetRotation().GetEulerAngles();

            ch.PreviousTranslation = ch.Translation;
            ch.PreviousRotation = ch.Rotation;
            ch.Translation = {position.GetX(), position.GetY(), position.GetZ()};
            ch.Rotation = glm::vec3(rotation.GetX(), rotation.GetY(), rotation.GetZ());
          }

          tc.Translation = glm::lerp(ch.PreviousTranslation, ch.Translation, interpolationFactor);
          tc.Rotation = glm::eulerAngles(glm::slerp(ch.PreviousRotation, ch.Rotation, interpolationFactor));
        }
        else {
          const JPH::Vec3 position = ch.Character->GetPosition();
          JPH::Vec3 rotation = ch.Character->GetRotation().GetEulerAngles();

          ch.PreviousTranslation = ch.Translation;
          ch.PreviousRotation = ch.Rotation;
          ch.Translation = {position.GetX(), position.GetY(), position.GetZ()};
          ch.Rotation = glm::vec3(rotation.GetX(), rotation.GetY(), rotation.GetZ());
          tc.Translation = ch.Translation;
          tc.Rotation = glm::eulerAngles(ch.Rotation);
        }
        PhysicsUtils::DebugDraw(this, e);
      }
    }
  }

  void Scene::IterateOverMeshNode(const Ref<Mesh>& mesh, const std::vector<Mesh::Node*>& node, Entity parent) {
    for (const auto child : node) {
      Entity entity = CreateEntity(child->Name).SetParent(parent);
      if (child->ContainsMesh) {
        entity.AddComponentI<MeshRendererComponent>(mesh).SubmesIndex = child->Index;
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
        entity.AddComponentI<MeshRendererComponent>(mesh).SubmesIndex = node->Index;
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
    OX_SCOPED_ZONE;
    const Entity newEntity = CreateEntity(entity.GetName());
    CopyComponentIfExists(AllComponents{}, newEntity, entity);
  }

  void Scene::OnRuntimeStart() {
    OX_SCOPED_ZONE;

    m_IsRunning = true;

    m_PhysicsFrameAccumulator = 0.0f;

    // Physics
    {
      OX_SCOPED_ZONE_N("Physics Start");
      Physics::Init();
      m_BodyActivationListener3D = new Physics3DBodyActivationListener();
      m_ContactListener3D = new Physics3DContactListener(this);
      const auto physicsSystem = Physics::GetPhysicsSystem();
      physicsSystem->SetBodyActivationListener(m_BodyActivationListener3D);
      physicsSystem->SetContactListener(m_ContactListener3D);

      // Rigidbodies
      {
        const auto group = m_Registry.group<RigidbodyComponent>(entt::get<TransformComponent>);
        for (auto&& [e, rb, tc] : group.each()) {
          rb.PreviousTranslation = rb.Translation = tc.Translation;
          rb.PreviousRotation = rb.Rotation = tc.Rotation;
          CreateRigidbody({e, this}, tc, rb);
        }
      }

      // Characters
      {
        const auto group = m_Registry.group<CharacterControllerComponent>(entt::get<TransformComponent>);
        for (auto&& [e, ch, tc] : group.each()) {
          CreateCharacterController({e, this}, tc, ch);
        }
      }

      physicsSystem->OptimizeBroadPhase();
    }
  }

  void Scene::OnRuntimeStop() {
    OX_SCOPED_ZONE;

    m_IsRunning = false;

    // Physics
    {
      JPH::BodyInterface& bodyInterface = Physics::GetPhysicsSystem()->GetBodyInterface();
      const auto rbView = m_Registry.view<RigidbodyComponent>();
      for (auto&& [e, rb] : rbView.each()) {
        if (rb.RuntimeBody) {
          const auto* body = static_cast<const JPH::Body*>(rb.RuntimeBody);
          bodyInterface.RemoveBody(body->GetID());
          bodyInterface.DestroyBody(body->GetID());
        }
      }
      const auto chView = m_Registry.view<CharacterControllerComponent>();
      for (auto&& [e, ch] : chView.each()) {
        if (ch.Character) {
          bodyInterface.RemoveBody(ch.Character->GetBodyID());
          ch.Character = nullptr;
        }
      }

      delete m_BodyActivationListener3D;
      delete m_ContactListener3D;
      m_BodyActivationListener3D = nullptr;
      m_ContactListener3D = nullptr;
      Physics::Shutdown();
    }
  }

  Entity Scene::FindEntity(const std::string_view& name) {
    OX_SCOPED_ZONE;
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
    OX_SCOPED_ZONE;
    return m_EntityMap.contains(uuid);
  }

  Entity Scene::GetEntityByUUID(UUID uuid) {
    OX_SCOPED_ZONE;
    const auto& it = m_EntityMap.find(uuid);
    if (it != m_EntityMap.end())
      return {it->second, this};

    return {};
  }

  Ref<Scene> Scene::Copy(const Ref<Scene>& other) {
    OX_SCOPED_ZONE;
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

    // Copy components (except IDComponent and TagComponent)
    CopyComponent(AllComponents{}, dstSceneRegistry, srcSceneRegistry, newScene->m_EntityMap);

    return newScene;
  }

  void Scene::OnContactAdded(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, const JPH::ContactSettings& settings) {
    for (const auto& system : m_Systems)
      system->OnContactAdded(this, body1, body2, manifold, settings);
  }

  void Scene::OnContactPersisted(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, JPH::ContactSettings& settings) {
    for (const auto& system : m_Systems)
      system->OnContactPersisted(this, body1, body2, manifold, settings);
  }

  void Scene::RenderScene() {
    m_SceneRenderer.Render();
  }

  void Scene::CreateRigidbody(Entity entity, const TransformComponent& transform, RigidbodyComponent& component) const {
    OX_SCOPED_ZONE;
    if (!m_IsRunning)
      return;

    auto& bodyInterface = Physics::GetBodyInterface();
    if (component.RuntimeBody) {
      bodyInterface.DestroyBody(static_cast<JPH::Body*>(component.RuntimeBody)->GetID());
      component.RuntimeBody = nullptr;
    }

    JPH::MutableCompoundShapeSettings compoundShapeSettings;
    float maxScaleComponent = glm::max(glm::max(transform.Scale.x, transform.Scale.y), transform.Scale.z);

    const auto& entityName = entity.GetComponent<TagComponent>().Tag;

    if (entity.HasComponent<BoxColliderComponent>()) {
      const auto& bc = entity.GetComponent<BoxColliderComponent>();
      const auto* mat = new PhysicsMaterial3D(entityName, JPH::ColorArg(255, 0, 0), bc.Friction, bc.Restitution);

      Vec3 scale = bc.Size;
      JPH::BoxShapeSettings shapeSettings({glm::abs(scale.x), glm::abs(scale.y), glm::abs(scale.z)}, 0.05f, mat);
      shapeSettings.SetDensity(glm::max(0.001f, bc.Density));

      compoundShapeSettings.AddShape({bc.Offset.x, bc.Offset.y, bc.Offset.z}, JPH::Quat::sIdentity(), shapeSettings.Create().Get());
    }

    if (entity.HasComponent<SphereColliderComponent>()) {
      const auto& sc = entity.GetComponent<SphereColliderComponent>();
      const auto* mat = new PhysicsMaterial3D(entityName, JPH::ColorArg(255, 0, 0), sc.Friction, sc.Restitution);

      float radius = 2.0f * sc.Radius * maxScaleComponent;
      JPH::SphereShapeSettings shapeSettings(glm::max(0.01f, radius), mat);
      shapeSettings.SetDensity(glm::max(0.001f, sc.Density));

      compoundShapeSettings.AddShape({sc.Offset.x, sc.Offset.y, sc.Offset.z}, JPH::Quat::sIdentity(), shapeSettings.Create().Get());
    }

    if (entity.HasComponent<CapsuleColliderComponent>()) {
      const auto& cc = entity.GetComponent<CapsuleColliderComponent>();
      const auto* mat = new PhysicsMaterial3D(entityName, JPH::ColorArg(255, 0, 0), cc.Friction, cc.Restitution);

      float radius = 2.0f * cc.Radius * maxScaleComponent;
      JPH::CapsuleShapeSettings shapeSettings(glm::max(0.01f, cc.Height) * 0.5f, glm::max(0.01f, radius), mat);
      shapeSettings.SetDensity(glm::max(0.001f, cc.Density));

      compoundShapeSettings.AddShape({cc.Offset.x, cc.Offset.y, cc.Offset.z}, JPH::Quat::sIdentity(), shapeSettings.Create().Get());
    }

    if (entity.HasComponent<TaperedCapsuleColliderComponent>()) {
      const auto& tcc = entity.GetComponent<TaperedCapsuleColliderComponent>();
      const auto* mat = new PhysicsMaterial3D(entityName, JPH::ColorArg(255, 0, 0), tcc.Friction, tcc.Restitution);

      float topRadius = 2.0f * tcc.TopRadius * maxScaleComponent;
      float bottomRadius = 2.0f * tcc.BottomRadius * maxScaleComponent;
      JPH::TaperedCapsuleShapeSettings shapeSettings(glm::max(0.01f, tcc.Height) * 0.5f, glm::max(0.01f, topRadius), glm::max(0.01f, bottomRadius), mat);
      shapeSettings.SetDensity(glm::max(0.001f, tcc.Density));

      compoundShapeSettings.AddShape({tcc.Offset.x, tcc.Offset.y, tcc.Offset.z}, JPH::Quat::sIdentity(), shapeSettings.Create().Get());
    }

    if (entity.HasComponent<CylinderColliderComponent>()) {
      const auto& cc = entity.GetComponent<CylinderColliderComponent>();
      const auto* mat = new PhysicsMaterial3D(entityName, JPH::ColorArg(255, 0, 0), cc.Friction, cc.Restitution);

      float radius = 2.0f * cc.Radius * maxScaleComponent;
      JPH::CylinderShapeSettings shapeSettings(glm::max(0.01f, cc.Height) * 0.5f, glm::max(0.01f, radius), 0.05f, mat);
      shapeSettings.SetDensity(glm::max(0.001f, cc.Density));

      compoundShapeSettings.AddShape({cc.Offset.x, cc.Offset.y, cc.Offset.z}, JPH::Quat::sIdentity(), shapeSettings.Create().Get());
    }

    // Body
    auto rotation = glm::quat(transform.Rotation);

    auto layer = entity.GetComponent<TagComponent>().Layer;
    uint8_t layerIndex = 1;	// Default Layer
    auto collisionMaskIt = Physics::LayerCollisionMask.find(layer);
    if (collisionMaskIt != Physics::LayerCollisionMask.end())
      layerIndex = collisionMaskIt->second.Index;

    JPH::BodyCreationSettings bodySettings(compoundShapeSettings.Create().Get(), {transform.Translation.x, transform.Translation.y, transform.Translation.z}, {rotation.x, rotation.y, rotation.z, rotation.w}, static_cast<JPH::EMotionType>(component.Type), layerIndex);

    JPH::MassProperties massProperties;
    massProperties.mMass = glm::max(0.01f, component.Mass);
    bodySettings.mMassPropertiesOverride = massProperties;
    bodySettings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    bodySettings.mAllowSleeping = component.AllowSleep;
    bodySettings.mLinearDamping = glm::max(0.0f, component.LinearDrag);
    bodySettings.mAngularDamping = glm::max(0.0f, component.AngularDrag);
    bodySettings.mMotionQuality = component.Continuous ? JPH::EMotionQuality::LinearCast : JPH::EMotionQuality::Discrete;
    bodySettings.mGravityFactor = component.GravityScale;

    bodySettings.mIsSensor = component.IsSensor;

    JPH::Body* body = bodyInterface.CreateBody(bodySettings);

    JPH::EActivation activation = component.Awake && component.Type != RigidbodyComponent::BodyType::Static ? JPH::EActivation::Activate : JPH::EActivation::DontActivate;
    bodyInterface.AddBody(body->GetID(), activation);

    component.RuntimeBody = body;
  }

  void Scene::CreateCharacterController(Entity entity, const TransformComponent& transform, CharacterControllerComponent& component) const {
    if (!m_IsRunning)
      return;
    auto position = JPH::Vec3(transform.Translation.x, transform.Translation.y, transform.Translation.z);
    const auto capsuleShape = JPH::RotatedTranslatedShapeSettings(
      JPH::Vec3(0, 0.5f * component.CharacterHeightStanding + component.CharacterRadiusStanding, 0),
      JPH::Quat::sIdentity(),
      new JPH::CapsuleShape(0.5f * component.CharacterHeightStanding, component.CharacterRadiusStanding)).Create().Get();

    // Create character
    const Ref<JPH::CharacterSettings> settings = CreateRef<JPH::CharacterSettings>();
    settings->mMaxSlopeAngle = JPH::DegreesToRadians(45.0f);
    settings->mLayer = PhysicsLayers::MOVING;
    settings->mShape = capsuleShape;
    settings->mFriction = component.Friction;
    settings->mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -component.CharacterRadiusStanding); // Accept contacts that touch the lower sphere of the capsule
    component.Character = new JPH::Character(settings.get(), position, JPH::Quat::sIdentity(), 0, Physics::GetPhysicsSystem());
    component.Character->AddToPhysicsSystem(JPH::EActivation::Activate);
  }

  void Scene::OnRuntimeUpdate(float deltaTime) {
    OX_SCOPED_ZONE;

    // Camera
    {
      OX_SCOPED_ZONE_N("Camera System");
      const auto group = m_Registry.view<TransformComponent, CameraComponent>();
      for (const auto entity : group) {
        auto [transform, camera] = group.get<TransformComponent, CameraComponent>(entity);
        camera.System->Update(transform.Translation, transform.Rotation);
        VulkanRenderer::SetCamera(*camera.System);
      }
    }

    RenderScene();

    {
      OX_SCOPED_ZONE_N("OnUpdate Systems");
      for (const auto& system : m_Systems) {
        system->OnUpdate(this, deltaTime);
      }
    }
    UpdatePhysics(deltaTime);
    {
      OX_SCOPED_ZONE_N("PostOnUpdate Systems");
      for (const auto& system : m_Systems) {
        system->PostOnUpdate(this, deltaTime);
      }
    }

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

  void Scene::OnImGuiRender(const float deltaTime) {
    for (const auto& system : m_Systems)
      system->OnImGuiRender(this, deltaTime);
  }

  void Scene::OnEditorUpdate(float deltaTime, Camera& camera) {
    RenderScene();

    const auto rbView = m_Registry.view<RigidbodyComponent>();
    for (auto&& [e, rb] : rbView.each()) {
      PhysicsUtils::DebugDraw(this, e);
    }
    const auto chView = m_Registry.view<CharacterControllerComponent>();
    for (auto&& [e, ch] : chView.each()) {
      PhysicsUtils::DebugDraw(this, e);
    }

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
  void Scene::OnComponentAdded<RigidbodyComponent>([[maybe_unused]] Entity entity, [[maybe_unused]] RigidbodyComponent& component) {
    CreateRigidbody(entity, entity.GetComponent<TransformComponent>(), component);
  }

  template <>
  void Scene::OnComponentAdded<BoxColliderComponent>([[maybe_unused]] Entity entity, [[maybe_unused]] BoxColliderComponent& component) {
    if (entity.HasComponent<RigidbodyComponent>())
      CreateRigidbody(entity, entity.GetComponent<TransformComponent>(), entity.GetComponent<RigidbodyComponent>());
  }

  template <>
  void Scene::OnComponentAdded<SphereColliderComponent>([[maybe_unused]] Entity entity, [[maybe_unused]] SphereColliderComponent& component) {
    if (entity.HasComponent<RigidbodyComponent>())
      CreateRigidbody(entity, entity.GetComponent<TransformComponent>(), entity.GetComponent<RigidbodyComponent>());
  }

  template <>
  void Scene::OnComponentAdded<CapsuleColliderComponent>([[maybe_unused]] Entity entity, [[maybe_unused]] CapsuleColliderComponent& component) {
    if (entity.HasComponent<RigidbodyComponent>())
      CreateRigidbody(entity, entity.GetComponent<TransformComponent>(), entity.GetComponent<RigidbodyComponent>());
  }

  template <>
  void Scene::OnComponentAdded<TaperedCapsuleColliderComponent>([[maybe_unused]] Entity entity, [[maybe_unused]] TaperedCapsuleColliderComponent& component) {
    if (entity.HasComponent<RigidbodyComponent>())
      CreateRigidbody(entity, entity.GetComponent<TransformComponent>(), entity.GetComponent<RigidbodyComponent>());
  }

  template <>
  void Scene::OnComponentAdded<CylinderColliderComponent>([[maybe_unused]] Entity entity, [[maybe_unused]] CylinderColliderComponent& component) {
    if (entity.HasComponent<RigidbodyComponent>())
      CreateRigidbody(entity, entity.GetComponent<TransformComponent>(), entity.GetComponent<RigidbodyComponent>());
  }

  template <>
  void Scene::OnComponentAdded<CharacterControllerComponent>([[maybe_unused]] Entity entity, [[maybe_unused]] CharacterControllerComponent& component) {
    CreateCharacterController(entity, entity.GetComponent<TransformComponent>(), component);
  }
}
