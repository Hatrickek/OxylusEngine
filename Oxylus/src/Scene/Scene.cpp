#include "Scene.h"

#include "Core/Entity.h"
#include "Render/Camera.h"
#include "Render/Vulkan/VulkanRenderer.h"
#include "Utils/Profiler.h"
#include "Utils/Timestep.h"

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
  this->SceneName = scene.SceneName;
  const auto pack = this->m_Registry.storage<entt::entity>().data();
  const auto packSize = this->m_Registry.storage<entt::entity>().size();
  this->m_Registry.storage<entt::entity>().push(pack, pack + packSize);
}

void Scene::Init() {
  // Renderer
  m_SceneRenderer = create_ref<SceneRenderer>();

  m_SceneRenderer->init(this);

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
  entity.AddComponentI<TagComponent>().tag = name.empty() ? "Entity" : name;
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
    if (!rb.runtime_body)
      continue;

    PhysicsUtils::DebugDraw(this, e);

    const auto* body = static_cast<const JPH::Body*>(rb.runtime_body);

    if (!bodyInterface.IsActive(body->GetID()))
      continue;

    if (rb.interpolation) {
      if (stepped) {
        JPH::Vec3 position = body->GetPosition();
        JPH::Vec3 rotation = body->GetRotation().GetEulerAngles();

        rb.previous_translation = rb.translation;
        rb.previous_rotation = rb.rotation;
        rb.translation = {position.GetX(), position.GetY(), position.GetZ()};
        rb.rotation = glm::vec3(rotation.GetX(), rotation.GetY(), rotation.GetZ());
      }

      tc.translation = glm::lerp(rb.previous_translation, rb.translation, interpolationFactor);
      tc.rotation = glm::eulerAngles(glm::slerp(rb.previous_rotation, rb.rotation, interpolationFactor));
    }
    else {
      const JPH::Vec3 position = body->GetPosition();
      JPH::Vec3 rotation = body->GetRotation().GetEulerAngles();

      rb.previous_translation = rb.translation;
      rb.previous_rotation = rb.rotation;
      rb.translation = {position.GetX(), position.GetY(), position.GetZ()};
      rb.rotation = glm::vec3(rotation.GetX(), rotation.GetY(), rotation.GetZ());
      tc.translation = rb.translation;
      tc.rotation = glm::eulerAngles(rb.rotation);
    }
  }

  // Character
  {
    const auto chView = m_Registry.view<TransformComponent, CharacterControllerComponent>();
    for (auto&& [e, tc, ch] : chView.each()) {
      ch.character->PostSimulation(ch.collision_tolerance);
      if (ch.interpolation) {
        if (stepped) {
          JPH::Vec3 position = ch.character->GetPosition();
          JPH::Vec3 rotation = ch.character->GetRotation().GetEulerAngles();

          ch.previous_translation = ch.translation;
          ch.previous_rotation = ch.Rotation;
          ch.translation = {position.GetX(), position.GetY(), position.GetZ()};
          ch.Rotation = glm::vec3(rotation.GetX(), rotation.GetY(), rotation.GetZ());
        }

        tc.translation = glm::lerp(ch.previous_translation, ch.translation, interpolationFactor);
        tc.rotation = glm::eulerAngles(glm::slerp(ch.previous_rotation, ch.Rotation, interpolationFactor));
      }
      else {
        const JPH::Vec3 position = ch.character->GetPosition();
        JPH::Vec3 rotation = ch.character->GetRotation().GetEulerAngles();

        ch.previous_translation = ch.translation;
        ch.previous_rotation = ch.Rotation;
        ch.translation = {position.GetX(), position.GetY(), position.GetZ()};
        ch.Rotation = glm::vec3(rotation.GetX(), rotation.GetY(), rotation.GetZ());
        tc.translation = ch.translation;
        tc.rotation = glm::eulerAngles(ch.Rotation);
      }
      PhysicsUtils::DebugDraw(this, e);
    }
  }
}

void Scene::IterateOverMeshNode(const Ref<Mesh>& mesh, const std::vector<Mesh::Node*>& node, Entity parent) {
  for (const auto child : node) {
    Entity entity = CreateEntity(child->name).SetParent(parent);
    if (child->contains_mesh) {
      entity.AddComponentI<MeshRendererComponent>(mesh).submesh_index = child->index;
      entity.GetComponent<MaterialComponent>().materials = mesh->get_materials_as_ref();
    }
    IterateOverMeshNode(mesh, child->children, entity);
  }
}

void Scene::CreateEntityWithMesh(const Ref<Mesh>& meshAsset) {
  for (const auto& node : meshAsset->nodes) {
    Entity entity = CreateEntity(node->name);
    if (node->contains_mesh) {
      entity.AddComponentI<MeshRendererComponent>(meshAsset).submesh_index = node->index;
      entity.GetComponent<MaterialComponent>().materials = meshAsset->get_materials_as_ref();
    }
    IterateOverMeshNode(meshAsset, node->children, entity);
  }
}

void Scene::DestroyEntity(const Entity entity) {
  entity.Deparent();
  const auto children = entity.GetComponent<RelationshipComponent>().children;

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
        rb.previous_translation = rb.translation = tc.translation;
        rb.previous_rotation = rb.rotation = tc.rotation;
        CreateRigidbody({e, this}, tc, rb);
      }
    }

    // Characters
    {
      const auto group = m_Registry.group<CharacterControllerComponent>(entt::get<TransformComponent>);
      for (auto&& [e, ch, tc] : group.each()) {
        CreateCharacterController(tc, ch);
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
      if (rb.runtime_body) {
        const auto* body = static_cast<const JPH::Body*>(rb.runtime_body);
        bodyInterface.RemoveBody(body->GetID());
        bodyInterface.DestroyBody(body->GetID());
      }
    }
    const auto chView = m_Registry.view<CharacterControllerComponent>();
    for (auto&& [e, ch] : chView.each()) {
      if (ch.character) {
        bodyInterface.RemoveBody(ch.character->GetBodyID());
        ch.character = nullptr;
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
    if (tag.tag == name) {
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
  Ref<Scene> newScene = create_ref<Scene>();

  auto& srcSceneRegistry = other->m_Registry;
  auto& dstSceneRegistry = newScene->m_Registry;

  // Create entities in new scene
  const auto view = srcSceneRegistry.view<IDComponent, TagComponent>();
  for (const auto e : view) {
    auto [id, tag] = view.get<IDComponent, TagComponent>(e);
    const auto& name = tag.tag;
    Entity newEntity = newScene->CreateEntityWithUUID(id.ID, name);
    newEntity.GetComponent<TagComponent>().enabled = tag.enabled;
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

void Scene::CreateRigidbody(Entity entity, const TransformComponent& transform, RigidbodyComponent& component) const {
  OX_SCOPED_ZONE;
  if (!m_IsRunning)
    return;

  auto& bodyInterface = Physics::GetBodyInterface();
  if (component.runtime_body) {
    bodyInterface.DestroyBody(static_cast<JPH::Body*>(component.runtime_body)->GetID());
    component.runtime_body = nullptr;
  }

  JPH::MutableCompoundShapeSettings compoundShapeSettings;
  float maxScaleComponent = glm::max(glm::max(transform.scale.x, transform.scale.y), transform.scale.z);

  const auto& entityName = entity.GetComponent<TagComponent>().tag;

  if (entity.HasComponent<BoxColliderComponent>()) {
    const auto& bc = entity.GetComponent<BoxColliderComponent>();
    const auto* mat = new PhysicsMaterial3D(entityName, JPH::ColorArg(255, 0, 0), bc.friction, bc.restitution);

    Vec3 scale = bc.size;
    JPH::BoxShapeSettings shapeSettings({glm::abs(scale.x), glm::abs(scale.y), glm::abs(scale.z)}, 0.05f, mat);
    shapeSettings.SetDensity(glm::max(0.001f, bc.density));

    compoundShapeSettings.AddShape({bc.offset.x, bc.offset.y, bc.offset.z}, JPH::Quat::sIdentity(), shapeSettings.Create().Get());
  }

  if (entity.HasComponent<SphereColliderComponent>()) {
    const auto& sc = entity.GetComponent<SphereColliderComponent>();
    const auto* mat = new PhysicsMaterial3D(entityName, JPH::ColorArg(255, 0, 0), sc.friction, sc.restitution);

    float radius = 2.0f * sc.radius * maxScaleComponent;
    JPH::SphereShapeSettings shapeSettings(glm::max(0.01f, radius), mat);
    shapeSettings.SetDensity(glm::max(0.001f, sc.density));

    compoundShapeSettings.AddShape({sc.offset.x, sc.offset.y, sc.offset.z}, JPH::Quat::sIdentity(), shapeSettings.Create().Get());
  }

  if (entity.HasComponent<CapsuleColliderComponent>()) {
    const auto& cc = entity.GetComponent<CapsuleColliderComponent>();
    const auto* mat = new PhysicsMaterial3D(entityName, JPH::ColorArg(255, 0, 0), cc.friction, cc.restitution);

    float radius = 2.0f * cc.radius * maxScaleComponent;
    JPH::CapsuleShapeSettings shapeSettings(glm::max(0.01f, cc.height) * 0.5f, glm::max(0.01f, radius), mat);
    shapeSettings.SetDensity(glm::max(0.001f, cc.density));

    compoundShapeSettings.AddShape({cc.offset.x, cc.offset.y, cc.offset.z}, JPH::Quat::sIdentity(), shapeSettings.Create().Get());
  }

  if (entity.HasComponent<TaperedCapsuleColliderComponent>()) {
    const auto& tcc = entity.GetComponent<TaperedCapsuleColliderComponent>();
    const auto* mat = new PhysicsMaterial3D(entityName, JPH::ColorArg(255, 0, 0), tcc.friction, tcc.restitution);

    float topRadius = 2.0f * tcc.top_radius * maxScaleComponent;
    float bottomRadius = 2.0f * tcc.bottom_radius * maxScaleComponent;
    JPH::TaperedCapsuleShapeSettings shapeSettings(glm::max(0.01f, tcc.height) * 0.5f, glm::max(0.01f, topRadius), glm::max(0.01f, bottomRadius), mat);
    shapeSettings.SetDensity(glm::max(0.001f, tcc.density));

    compoundShapeSettings.AddShape({tcc.offset.x, tcc.offset.y, tcc.offset.z}, JPH::Quat::sIdentity(), shapeSettings.Create().Get());
  }

  if (entity.HasComponent<CylinderColliderComponent>()) {
    const auto& cc = entity.GetComponent<CylinderColliderComponent>();
    const auto* mat = new PhysicsMaterial3D(entityName, JPH::ColorArg(255, 0, 0), cc.friction, cc.restitution);

    float radius = 2.0f * cc.radius * maxScaleComponent;
    JPH::CylinderShapeSettings shapeSettings(glm::max(0.01f, cc.height) * 0.5f, glm::max(0.01f, radius), 0.05f, mat);
    shapeSettings.SetDensity(glm::max(0.001f, cc.density));

    compoundShapeSettings.AddShape({cc.offset.x, cc.offset.y, cc.offset.z}, JPH::Quat::sIdentity(), shapeSettings.Create().Get());
  }

  // Body
  auto rotation = glm::quat(transform.rotation);

  auto layer = entity.GetComponent<TagComponent>().layer;
  uint8_t layerIndex = 1;	// Default Layer
  auto collisionMaskIt = Physics::LayerCollisionMask.find(layer);
  if (collisionMaskIt != Physics::LayerCollisionMask.end())
    layerIndex = collisionMaskIt->second.Index;

  JPH::BodyCreationSettings bodySettings(compoundShapeSettings.Create().Get(), {transform.translation.x, transform.translation.y, transform.translation.z}, {rotation.x, rotation.y, rotation.z, rotation.w}, static_cast<JPH::EMotionType>(component.type), layerIndex);

  JPH::MassProperties massProperties;
  massProperties.mMass = glm::max(0.01f, component.mass);
  bodySettings.mMassPropertiesOverride = massProperties;
  bodySettings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
  bodySettings.mAllowSleeping = component.allow_sleep;
  bodySettings.mLinearDamping = glm::max(0.0f, component.linear_drag);
  bodySettings.mAngularDamping = glm::max(0.0f, component.angular_drag);
  bodySettings.mMotionQuality = component.continuous ? JPH::EMotionQuality::LinearCast : JPH::EMotionQuality::Discrete;
  bodySettings.mGravityFactor = component.gravity_scale;

  bodySettings.mIsSensor = component.is_sensor;

  JPH::Body* body = bodyInterface.CreateBody(bodySettings);

  JPH::EActivation activation = component.awake && component.type != RigidbodyComponent::BodyType::Static ? JPH::EActivation::Activate : JPH::EActivation::DontActivate;
  bodyInterface.AddBody(body->GetID(), activation);

  component.runtime_body = body;
}

void Scene::CreateCharacterController(const TransformComponent& transform, CharacterControllerComponent& component) const {
  if (!m_IsRunning)
    return;
  auto position = JPH::Vec3(transform.translation.x, transform.translation.y, transform.translation.z);
  const auto capsuleShape = JPH::RotatedTranslatedShapeSettings(
    JPH::Vec3(0, 0.5f * component.character_height_standing + component.character_radius_standing, 0),
    JPH::Quat::sIdentity(),
    new JPH::CapsuleShape(0.5f * component.character_height_standing, component.character_radius_standing)).Create().Get();

  // Create character
  const Ref<JPH::CharacterSettings> settings = create_ref<JPH::CharacterSettings>();
  settings->mMaxSlopeAngle = JPH::DegreesToRadians(45.0f);
  settings->mLayer = PhysicsLayers::MOVING;
  settings->mShape = capsuleShape;
  settings->mFriction = 0.0f; // For now this is not set. 
  settings->mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -component.character_radius_standing); // Accept contacts that touch the lower sphere of the capsule
  component.character = new JPH::Character(settings.get(), position, JPH::Quat::sIdentity(), 0, Physics::GetPhysicsSystem());
  component.character->AddToPhysicsSystem(JPH::EActivation::Activate);
}

void Scene::OnRuntimeUpdate(float deltaTime) {
  OX_SCOPED_ZONE;

  // Camera
  {
    OX_SCOPED_ZONE_N("Camera System");
    const auto group = m_Registry.view<TransformComponent, CameraComponent>();
    for (const auto entity : group) {
      auto [transform, camera] = group.get<TransformComponent, CameraComponent>(entity);
      camera.system->Update(transform.translation, transform.rotation);
      VulkanRenderer::set_camera(*camera.system);
    }
  }

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
      ac.listener = create_ref<AudioListener>();
      if (ac.active) {
        const glm::mat4 inverted = glm::inverse(Entity(e, this).GetWorldTransform());
        const glm::vec3 forward = normalize(glm::vec3(inverted[2]));
        ac.listener->SetConfig(ac.config);
        ac.listener->SetPosition(tc.translation);
        ac.listener->SetDirection(-forward);
        break;
      }
    }

    const auto sourceView = m_Registry.group<AudioSourceComponent>(entt::get<TransformComponent>);
    for (auto&& [e, ac, tc] : sourceView.each()) {
      if (ac.source) {
        const glm::mat4 inverted = glm::inverse(Entity(e, this).GetWorldTransform());
        const glm::vec3 forward = normalize(glm::vec3(inverted[2]));
        ac.source->SetConfig(ac.config);
        ac.source->SetPosition(tc.translation);
        ac.source->SetDirection(forward);
        if (ac.config.PlayOnAwake)
          ac.source->Play();
      }
    }
  }
}

void Scene::OnImGuiRender(const float deltaTime) {
  for (const auto& system : m_Systems)
    system->OnImGuiRender(this, deltaTime);
}

void Scene::OnEditorUpdate(float deltaTime, Camera& camera) {
  const auto rbView = m_Registry.view<RigidbodyComponent>();
  for (auto&& [e, rb] : rbView.each()) {
    PhysicsUtils::DebugDraw(this, e);
  }
  const auto chView = m_Registry.view<CharacterControllerComponent>();
  for (auto&& [e, ch] : chView.each()) {
    PhysicsUtils::DebugDraw(this, e);
  }

  VulkanRenderer::set_camera(camera);
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
  if (component.materials.empty()) {
    if (entity.HasComponent<MeshRendererComponent>())
      component.materials = entity.GetComponent<MeshRendererComponent>().mesh_geometry->get_materials_as_ref();
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
void Scene::OnComponentAdded<RigidbodyComponent>(Entity entity, RigidbodyComponent& component) {
  CreateRigidbody(entity, entity.GetComponent<TransformComponent>(), component);
}

template <>
void Scene::OnComponentAdded<BoxColliderComponent>(Entity entity, BoxColliderComponent& component) {
  if (entity.HasComponent<RigidbodyComponent>())
    CreateRigidbody(entity, entity.GetComponent<TransformComponent>(), entity.GetComponent<RigidbodyComponent>());
}

template <>
void Scene::OnComponentAdded<SphereColliderComponent>(Entity entity, SphereColliderComponent& component) {
  if (entity.HasComponent<RigidbodyComponent>())
    CreateRigidbody(entity, entity.GetComponent<TransformComponent>(), entity.GetComponent<RigidbodyComponent>());
}

template <>
void Scene::OnComponentAdded<CapsuleColliderComponent>(Entity entity, CapsuleColliderComponent& component) {
  if (entity.HasComponent<RigidbodyComponent>())
    CreateRigidbody(entity, entity.GetComponent<TransformComponent>(), entity.GetComponent<RigidbodyComponent>());
}

template <>
void Scene::OnComponentAdded<TaperedCapsuleColliderComponent>(Entity entity, TaperedCapsuleColliderComponent& component) {
  if (entity.HasComponent<RigidbodyComponent>())
    CreateRigidbody(entity, entity.GetComponent<TransformComponent>(), entity.GetComponent<RigidbodyComponent>());
}

template <>
void Scene::OnComponentAdded<CylinderColliderComponent>(Entity entity, CylinderColliderComponent& component) {
  if (entity.HasComponent<RigidbodyComponent>())
    CreateRigidbody(entity, entity.GetComponent<TransformComponent>(), entity.GetComponent<RigidbodyComponent>());
}

template <>
void Scene::OnComponentAdded<CharacterControllerComponent>(Entity entity, CharacterControllerComponent& component) {
  CreateCharacterController(entity.GetComponent<TransformComponent>(), component);
}

template <>
void Scene::OnComponentAdded<CustomComponent>(Entity entity, CustomComponent& component) { }
}
