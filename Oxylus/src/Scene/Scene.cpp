#include "Scene.h"

#include "Core/Entity.h"
#include "Render/Camera.h"
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

#include "Render/RenderPipeline.h"

namespace Oxylus {
Scene::Scene() {
  init();
}

Scene::Scene(std::string name) : scene_name(std::move(name)) { }

Scene::~Scene() {
  if (is_running)
    on_runtime_stop();
}

Scene::Scene(const Scene& scene) {
  this->scene_name = scene.scene_name;
  const auto pack = this->m_registry.storage<entt::entity>().data();
  const auto packSize = this->m_registry.storage<entt::entity>().size();
  this->m_registry.storage<entt::entity>().push(pack, pack + packSize);
}

void Scene::init() {
  // Renderer
  scene_renderer = create_ref<SceneRenderer>(this);

  scene_renderer->init();

  // Systems
  for (const auto& system : systems) {
    system->on_init();
  }
}

Entity Scene::create_entity(const std::string& name) {
  return create_entity_with_uuid(UUID(), name);
}

Entity Scene::create_entity_with_uuid(UUID uuid, const std::string& name) {
  Entity entity = {m_registry.create(), this};
  entity_map.emplace(uuid, entity);
  entity.add_component_internal<IDComponent>(uuid);
  entity.add_component_internal<RelationshipComponent>();
  entity.add_component_internal<TransformComponent>();
  entity.add_component_internal<TagComponent>().tag = name.empty() ? "Entity" : name;
  return entity;
}

void Scene::update_physics(Timestep delta_time) {
  OX_SCOPED_ZONE;
  // Minimum stable value is 16.0
  constexpr float physicsStepRate = 64.0f;
  constexpr float physicsTs = 1.0f / physicsStepRate;

  bool stepped = false;
  physics_frame_accumulator += delta_time;

  while (physics_frame_accumulator >= physicsTs) {
    Physics::Step(physicsTs);

    physics_frame_accumulator -= physicsTs;
    stepped = true;
  }

  const float interpolationFactor = physics_frame_accumulator / physicsTs;

  const auto& bodyInterface = Physics::GetPhysicsSystem()->GetBodyInterface();
  const auto view = m_registry.group<RigidbodyComponent>(entt::get<TransformComponent>);
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
    const auto chView = m_registry.view<TransformComponent, CharacterControllerComponent>();
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

void Scene::iterate_over_mesh_node(const Ref<Mesh>& mesh, const std::vector<Mesh::Node*>& node, Entity parent) {
  for (const auto child : node) {
    Entity entity = create_entity(child->name).set_parent(parent);
    if (child->mesh_data) {
      const auto it = std::find(mesh->linear_nodes.begin(), mesh->linear_nodes.end(), child);
      const auto index = std::distance(mesh->linear_nodes.begin(), it);

      entity.add_component_internal<MeshRendererComponent>(mesh).submesh_index = index;
      entity.get_component<MaterialComponent>().materials = mesh->get_materials_as_ref();
    }
    iterate_over_mesh_node(mesh, child->children, entity);
  }
}

void Scene::create_entity_with_mesh(const Ref<Mesh>& mesh_asset) {
  for (const auto& node : mesh_asset->nodes) {
    Entity entity = create_entity(node->name);
    if (node->mesh_data) {
      entity.add_component_internal<MeshRendererComponent>(mesh_asset).submesh_index = node->index;
      entity.get_component<MaterialComponent>().materials = mesh_asset->get_materials_as_ref();
    }
    iterate_over_mesh_node(mesh_asset, node->children, entity);
  }
}

void Scene::destroy_entity(const Entity entity) {
  entity.deparent();
  const auto children = entity.get_component<RelationshipComponent>().children;

  for (size_t i = 0; i < children.size(); i++) {
    if (const Entity childEntity = get_entity_by_uuid(children[i]))
      destroy_entity(childEntity);
  }

  entity_map.erase(entity.get_uuid());
  m_registry.destroy(entity);
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
    if (src.has_component<Component>())
      dst.add_or_replace_component<Component>(src.get_component<Component>());
  }(), ...);
}

template <typename... Component>
static void CopyComponentIfExists(ComponentGroup<Component...>,
                                  Entity dst,
                                  Entity src) {
  CopyComponentIfExists<Component...>(dst, src);
}

void Scene::duplicate_entity(Entity entity) {
  OX_SCOPED_ZONE;
  const Entity newEntity = create_entity(entity.get_name());
  CopyComponentIfExists(AllComponents{}, newEntity, entity);
}

void Scene::on_runtime_start() {
  OX_SCOPED_ZONE;

  is_running = true;

  physics_frame_accumulator = 0.0f;

  // Physics
  {
    OX_SCOPED_ZONE_N("Physics Start");
    Physics::Init();
    body_activation_listener_3d = new Physics3DBodyActivationListener();
    contact_listener_3d = new Physics3DContactListener(this);
    const auto physicsSystem = Physics::GetPhysicsSystem();
    physicsSystem->SetBodyActivationListener(body_activation_listener_3d);
    physicsSystem->SetContactListener(contact_listener_3d);

    // Rigidbodies
    {
      const auto group = m_registry.group<RigidbodyComponent>(entt::get<TransformComponent>);
      for (auto&& [e, rb, tc] : group.each()) {
        rb.previous_translation = rb.translation = tc.translation;
        rb.previous_rotation = rb.rotation = tc.rotation;
        create_rigidbody({e, this}, tc, rb);
      }
    }

    // Characters
    {
      const auto group = m_registry.group<CharacterControllerComponent>(entt::get<TransformComponent>);
      for (auto&& [e, ch, tc] : group.each()) {
        create_character_controller(tc, ch);
      }
    }

    physicsSystem->OptimizeBroadPhase();
  }
}

void Scene::on_runtime_stop() {
  OX_SCOPED_ZONE;

  is_running = false;

  // Physics
  {
    JPH::BodyInterface& bodyInterface = Physics::GetPhysicsSystem()->GetBodyInterface();
    const auto rbView = m_registry.view<RigidbodyComponent>();
    for (auto&& [e, rb] : rbView.each()) {
      if (rb.runtime_body) {
        const auto* body = static_cast<const JPH::Body*>(rb.runtime_body);
        bodyInterface.RemoveBody(body->GetID());
        bodyInterface.DestroyBody(body->GetID());
      }
    }
    const auto chView = m_registry.view<CharacterControllerComponent>();
    for (auto&& [e, ch] : chView.each()) {
      if (ch.character) {
        bodyInterface.RemoveBody(ch.character->GetBodyID());
        ch.character = nullptr;
      }
    }

    delete body_activation_listener_3d;
    delete contact_listener_3d;
    body_activation_listener_3d = nullptr;
    contact_listener_3d = nullptr;
  }
}

Entity Scene::find_entity(const std::string_view& name) {
  OX_SCOPED_ZONE;
  const auto group = m_registry.view<TagComponent>();
  for (const auto& entity : group) {
    auto& tag = group.get<TagComponent>(entity);
    if (tag.tag == name) {
      return Entity{entity, this};
    }
  }
  return {};
}

bool Scene::has_entity(UUID uuid) const {
  OX_SCOPED_ZONE;
  return entity_map.contains(uuid);
}

Entity Scene::get_entity_by_uuid(UUID uuid) {
  OX_SCOPED_ZONE;
  const auto& it = entity_map.find(uuid);
  if (it != entity_map.end())
    return {it->second, this};

  return {};
}

Ref<Scene> Scene::copy(const Ref<Scene>& other) {
  OX_SCOPED_ZONE;
  Ref<Scene> newScene = create_ref<Scene>();

  newScene->scene_renderer = other->scene_renderer;

  auto& srcSceneRegistry = other->m_registry;
  auto& dstSceneRegistry = newScene->m_registry;

  // Create entities in new scene
  const auto view = srcSceneRegistry.view<IDComponent, TagComponent>();
  for (const auto e : view) {
    auto [id, tag] = view.get<IDComponent, TagComponent>(e);
    const auto& name = tag.tag;
    Entity newEntity = newScene->create_entity_with_uuid(id.ID, name);
    newEntity.get_component<TagComponent>().enabled = tag.enabled;
  }

  for (const auto e : view) {
    Entity src = {e, other.get()};
    Entity dst = newScene->get_entity_by_uuid(view.get<IDComponent>(e).ID);
    if (Entity srcParent = src.get_parent())
      dst.set_parent(newScene->get_entity_by_uuid(srcParent.get_uuid()));
  }

  // Copy components (except IDComponent and TagComponent)
  CopyComponent(AllComponents{}, dstSceneRegistry, srcSceneRegistry, newScene->entity_map);

  return newScene;
}

void Scene::on_contact_added(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, const JPH::ContactSettings& settings) {
  for (const auto& system : systems)
    system->on_contact_added(this, body1, body2, manifold, settings);
}

void Scene::on_contact_persisted(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, JPH::ContactSettings& settings) {
  for (const auto& system : systems)
    system->on_contact_persisted(this, body1, body2, manifold, settings);
}

void Scene::on_key_pressed(const KeyCode key) const {
  for (const auto& system : systems)
    system->on_key_pressed(key);
}

void Scene::on_key_released(const KeyCode key) const {
  for (const auto& system : systems)
    system->on_key_released(key);
}

void Scene::create_rigidbody(Entity entity, const TransformComponent& transform, RigidbodyComponent& component) const {
  OX_SCOPED_ZONE;
  if (!is_running)
    return;

  auto& bodyInterface = Physics::GetBodyInterface();
  if (component.runtime_body) {
    bodyInterface.DestroyBody(static_cast<JPH::Body*>(component.runtime_body)->GetID());
    component.runtime_body = nullptr;
  }

  JPH::MutableCompoundShapeSettings compoundShapeSettings;
  float maxScaleComponent = glm::max(glm::max(transform.scale.x, transform.scale.y), transform.scale.z);

  const auto& entityName = entity.get_component<TagComponent>().tag;

  if (entity.has_component<BoxColliderComponent>()) {
    const auto& bc = entity.get_component<BoxColliderComponent>();
    const auto* mat = new PhysicsMaterial3D(entityName, JPH::ColorArg(255, 0, 0), bc.friction, bc.restitution);

    Vec3 scale = bc.size;
    JPH::BoxShapeSettings shapeSettings({glm::abs(scale.x), glm::abs(scale.y), glm::abs(scale.z)}, 0.05f, mat);
    shapeSettings.SetDensity(glm::max(0.001f, bc.density));

    compoundShapeSettings.AddShape({bc.offset.x, bc.offset.y, bc.offset.z}, JPH::Quat::sIdentity(), shapeSettings.Create().Get());
  }

  if (entity.has_component<SphereColliderComponent>()) {
    const auto& sc = entity.get_component<SphereColliderComponent>();
    const auto* mat = new PhysicsMaterial3D(entityName, JPH::ColorArg(255, 0, 0), sc.friction, sc.restitution);

    float radius = 2.0f * sc.radius * maxScaleComponent;
    JPH::SphereShapeSettings shapeSettings(glm::max(0.01f, radius), mat);
    shapeSettings.SetDensity(glm::max(0.001f, sc.density));

    compoundShapeSettings.AddShape({sc.offset.x, sc.offset.y, sc.offset.z}, JPH::Quat::sIdentity(), shapeSettings.Create().Get());
  }

  if (entity.has_component<CapsuleColliderComponent>()) {
    const auto& cc = entity.get_component<CapsuleColliderComponent>();
    const auto* mat = new PhysicsMaterial3D(entityName, JPH::ColorArg(255, 0, 0), cc.friction, cc.restitution);

    float radius = 2.0f * cc.radius * maxScaleComponent;
    JPH::CapsuleShapeSettings shapeSettings(glm::max(0.01f, cc.height) * 0.5f, glm::max(0.01f, radius), mat);
    shapeSettings.SetDensity(glm::max(0.001f, cc.density));

    compoundShapeSettings.AddShape({cc.offset.x, cc.offset.y, cc.offset.z}, JPH::Quat::sIdentity(), shapeSettings.Create().Get());
  }

  if (entity.has_component<TaperedCapsuleColliderComponent>()) {
    const auto& tcc = entity.get_component<TaperedCapsuleColliderComponent>();
    const auto* mat = new PhysicsMaterial3D(entityName, JPH::ColorArg(255, 0, 0), tcc.friction, tcc.restitution);

    float topRadius = 2.0f * tcc.top_radius * maxScaleComponent;
    float bottomRadius = 2.0f * tcc.bottom_radius * maxScaleComponent;
    JPH::TaperedCapsuleShapeSettings shapeSettings(glm::max(0.01f, tcc.height) * 0.5f, glm::max(0.01f, topRadius), glm::max(0.01f, bottomRadius), mat);
    shapeSettings.SetDensity(glm::max(0.001f, tcc.density));

    compoundShapeSettings.AddShape({tcc.offset.x, tcc.offset.y, tcc.offset.z}, JPH::Quat::sIdentity(), shapeSettings.Create().Get());
  }

  if (entity.has_component<CylinderColliderComponent>()) {
    const auto& cc = entity.get_component<CylinderColliderComponent>();
    const auto* mat = new PhysicsMaterial3D(entityName, JPH::ColorArg(255, 0, 0), cc.friction, cc.restitution);

    float radius = 2.0f * cc.radius * maxScaleComponent;
    JPH::CylinderShapeSettings shapeSettings(glm::max(0.01f, cc.height) * 0.5f, glm::max(0.01f, radius), 0.05f, mat);
    shapeSettings.SetDensity(glm::max(0.001f, cc.density));

    compoundShapeSettings.AddShape({cc.offset.x, cc.offset.y, cc.offset.z}, JPH::Quat::sIdentity(), shapeSettings.Create().Get());
  }

  // Body
  auto rotation = glm::quat(transform.rotation);

  auto layer = entity.get_component<TagComponent>().layer;
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

void Scene::create_character_controller(const TransformComponent& transform, CharacterControllerComponent& component) const {
  if (!is_running)
    return;
  const auto position = JPH::Vec3(transform.translation.x, transform.translation.y, transform.translation.z);
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

void Scene::on_runtime_update(float delta_time) {
  OX_SCOPED_ZONE;

  // Camera
  {
    OX_SCOPED_ZONE_N("Camera System");
    const auto group = m_registry.view<TransformComponent, CameraComponent>();
    for (const auto entity : group) {
      auto [transform, camera] = group.get<TransformComponent, CameraComponent>(entity);
      camera.system->update(transform.translation, transform.rotation);
      scene_renderer->get_render_pipeline()->on_register_camera(camera.system.get());
    }
  }

  scene_renderer->update();

  {
    OX_SCOPED_ZONE_N("OnUpdate Systems");
    for (const auto& system : systems) {
      system->on_update(this, delta_time);
    }
  }
  update_physics(delta_time);
  {
    OX_SCOPED_ZONE_N("PostOnUpdate Systems");
    for (const auto& system : systems) {
      system->post_on_update(this, delta_time);
    }
  }

  // Audio
  {
    const auto listenerView = m_registry.group<AudioListenerComponent>(entt::get<TransformComponent>);
    for (auto&& [e, ac, tc] : listenerView.each()) {
      ac.listener = create_ref<AudioListener>();
      if (ac.active) {
        const glm::mat4 inverted = glm::inverse(Entity(e, this).get_world_transform());
        const glm::vec3 forward = normalize(glm::vec3(inverted[2]));
        ac.listener->SetConfig(ac.config);
        ac.listener->SetPosition(tc.translation);
        ac.listener->SetDirection(-forward);
        break;
      }
    }

    const auto sourceView = m_registry.group<AudioSourceComponent>(entt::get<TransformComponent>);
    for (auto&& [e, ac, tc] : sourceView.each()) {
      if (ac.source) {
        const glm::mat4 inverted = glm::inverse(Entity(e, this).get_world_transform());
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

void Scene::on_imgui_render(const float delta_time) {
  for (const auto& system : systems)
    system->on_imgui_render(this, delta_time);
}

void Scene::on_editor_update(float delta_time, Camera& camera) {
  scene_renderer->get_render_pipeline()->on_register_camera(&camera);
  scene_renderer->update();

  const auto rbView = m_registry.view<RigidbodyComponent>();
  for (auto&& [e, rb] : rbView.each()) {
    PhysicsUtils::DebugDraw(this, e);
  }
  const auto chView = m_registry.view<CharacterControllerComponent>();
  for (auto&& [e, ch] : chView.each()) {
    PhysicsUtils::DebugDraw(this, e);
  }
}

template <typename T>
void Scene::on_component_added(Entity entity, T& component) {
  static_assert(sizeof(T) == 0);
}

template <>
void Scene::on_component_added<TagComponent>(Entity entity, TagComponent& component) { }

template <>
void Scene::on_component_added<IDComponent>(Entity entity, IDComponent& component) { }

template <>
void Scene::on_component_added<RelationshipComponent>(Entity entity, RelationshipComponent& component) { }

template <>
void Scene::on_component_added<PrefabComponent>(Entity entity, PrefabComponent& component) { }

template <>
void Scene::on_component_added<TransformComponent>(Entity entity, TransformComponent& component) { }

template <>
void Scene::on_component_added<CameraComponent>(Entity entity, CameraComponent& component) { }

template <>
void Scene::on_component_added<AudioSourceComponent>(Entity entity, AudioSourceComponent& component) { }

template <>
void Scene::on_component_added<AudioListenerComponent>(Entity entity, AudioListenerComponent& component) { }

template <>
void Scene::on_component_added<MeshRendererComponent>(Entity entity, MeshRendererComponent& component) {
  entity.add_component_internal<MaterialComponent>();
}

template <>
void Scene::on_component_added<SkyLightComponent>(Entity entity, SkyLightComponent& component) { }

template <>
void Scene::on_component_added<MaterialComponent>(Entity entity, MaterialComponent& component) {
  if (component.materials.empty()) {
    if (entity.has_component<MeshRendererComponent>())
      component.materials = entity.get_component<MeshRendererComponent>().mesh_geometry->get_materials_as_ref();
  }
}

template <>
void Scene::on_component_added<LightComponent>(Entity entity, LightComponent& component) { }

template <>
void Scene::on_component_added<PostProcessProbe>(Entity entity, PostProcessProbe& component) { }

template <>
void Scene::on_component_added<ParticleSystemComponent>(Entity entity,
                                                        ParticleSystemComponent& component) { }

template <>
void Scene::on_component_added<RigidbodyComponent>(Entity entity, RigidbodyComponent& component) {
  create_rigidbody(entity, entity.get_component<TransformComponent>(), component);
}

template <>
void Scene::on_component_added<BoxColliderComponent>(Entity entity, BoxColliderComponent& component) {
  if (entity.has_component<RigidbodyComponent>())
    create_rigidbody(entity, entity.get_component<TransformComponent>(), entity.get_component<RigidbodyComponent>());
}

template <>
void Scene::on_component_added<SphereColliderComponent>(Entity entity, SphereColliderComponent& component) {
  if (entity.has_component<RigidbodyComponent>())
    create_rigidbody(entity, entity.get_component<TransformComponent>(), entity.get_component<RigidbodyComponent>());
}

template <>
void Scene::on_component_added<CapsuleColliderComponent>(Entity entity, CapsuleColliderComponent& component) {
  if (entity.has_component<RigidbodyComponent>())
    create_rigidbody(entity, entity.get_component<TransformComponent>(), entity.get_component<RigidbodyComponent>());
}

template <>
void Scene::on_component_added<TaperedCapsuleColliderComponent>(Entity entity, TaperedCapsuleColliderComponent& component) {
  if (entity.has_component<RigidbodyComponent>())
    create_rigidbody(entity, entity.get_component<TransformComponent>(), entity.get_component<RigidbodyComponent>());
}

template <>
void Scene::on_component_added<CylinderColliderComponent>(Entity entity, CylinderColliderComponent& component) {
  if (entity.has_component<RigidbodyComponent>())
    create_rigidbody(entity, entity.get_component<TransformComponent>(), entity.get_component<RigidbodyComponent>());
}

template <>
void Scene::on_component_added<CharacterControllerComponent>(Entity entity, CharacterControllerComponent& component) {
  create_character_controller(entity.get_component<TransformComponent>(), component);
}

template <>
void Scene::on_component_added<CustomComponent>(Entity entity, CustomComponent& component) { }
}
