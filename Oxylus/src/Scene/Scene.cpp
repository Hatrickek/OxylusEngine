#include "Scene.h"

#include "Core/Entity.h"
#include "Render/Camera.h"
#include "Utils/Profiler.h"
#include "Utils/Timestep.h"

#include <glm/glm.hpp>

#include <unordered_map>
#include <glm/gtc/type_ptr.hpp>

#include "SceneRenderer.h"

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

Scene::Scene(std::string name) : scene_name(std::move(name)) {
  init();
}

Scene::Scene(const Ref<RenderPipeline>& render_pipeline) {
  init(render_pipeline);
}

Scene::~Scene() {
  if (is_running)
    on_runtime_stop();
}

Scene::Scene(const Scene& scene) {
  this->scene_name = scene.scene_name;
  const auto pack = this->m_registry.storage<entt::entity>().data();
  const auto pack_size = this->m_registry.storage<entt::entity>().size();
  this->m_registry.storage<entt::entity>().push(pack, pack + pack_size);
}

void Scene::init(const Ref<RenderPipeline>& render_pipeline) {
  // Renderer
  scene_renderer = create_ref<SceneRenderer>(this);

  scene_renderer->set_render_pipeline(render_pipeline);
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
  OX_SCOPED_ZONE;
  Entity entity = {m_registry.create(), this};
  entity_map.emplace(uuid, entity);
  entity.add_component_internal<IDComponent>(uuid);
  entity.add_component_internal<RelationshipComponent>();
  entity.add_component_internal<TransformComponent>();
  entity.add_component_internal<TagComponent>().tag = name.empty() ? "Entity" : name;
  return entity;
}

void Scene::load_mesh(const Ref<Mesh>& mesh) {
  OX_SCOPED_ZONE;
  const auto root_entity = create_entity(mesh->name);

  for (const auto& node : mesh->linear_nodes) {
    auto node_entity = create_entity(node->name);
    node_entity.get_transform().set_from_matrix(node->get_matrix());
    node_entity.set_parent(root_entity);
    auto& mesh_component = node_entity.add_component<MeshComponent>(mesh);
    auto& material_component = node_entity.add_component<MaterialComponent>();
    material_component.materials = mesh->materials;

    if (node->mesh_data) {
      for (const auto& primitive : node->mesh_data->primitives) {
        mesh_component.subsets.emplace_back(MeshComponent::MeshSubset{
          .index_count = primitive->index_count,
          .first_index = primitive->first_index,
          .material = mesh->materials[primitive->material_index],
          .material_index = primitive->material_index
        });
      }
    }
  }
}

void Scene::update_physics(const Timestep& delta_time) {
  OX_SCOPED_ZONE;
  // Minimum stable value is 16.0
  constexpr float physics_step_rate = 50.0f;
  constexpr float physics_ts = 1.0f / physics_step_rate;

  bool stepped = false;
  physics_frame_accumulator += delta_time;

  while (physics_frame_accumulator >= physics_ts) {
    Physics::step(physics_ts);

    {
      OX_SCOPED_ZONE_N("OnFixedUpdate Systems");
      for (const auto& system : systems) {
        system->on_fixed_update(this, physics_ts);
      }
    }

    physics_frame_accumulator -= physics_ts;
    stepped = true;
  }

  const float interpolation_factor = physics_frame_accumulator / physics_ts;

  const auto& body_interface = Physics::get_physics_system()->GetBodyInterface();
  const auto view = m_registry.group<RigidbodyComponent>(entt::get<TransformComponent>);
  for (auto&& [e, rb, tc] : view.each()) {
    if (!rb.runtime_body)
      continue;

    PhysicsUtils::DebugDraw(this, e);

    const auto* body = static_cast<const JPH::Body*>(rb.runtime_body);

    if (!body_interface.IsActive(body->GetID()))
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

      tc.position = glm::lerp(rb.previous_translation, rb.translation, interpolation_factor);
      tc.rotation = glm::eulerAngles(glm::slerp(rb.previous_rotation, rb.rotation, interpolation_factor));
    }
    else {
      const JPH::Vec3 position = body->GetPosition();
      JPH::Vec3 rotation = body->GetRotation().GetEulerAngles();

      rb.previous_translation = rb.translation;
      rb.previous_rotation = rb.rotation;
      rb.translation = {position.GetX(), position.GetY(), position.GetZ()};
      rb.rotation = glm::vec3(rotation.GetX(), rotation.GetY(), rotation.GetZ());
      tc.position = rb.translation;
      tc.rotation = glm::eulerAngles(rb.rotation);
    }
  }

  // Character
  {
    const auto ch_view = m_registry.view<TransformComponent, CharacterControllerComponent>();
    for (auto&& [e, tc, ch] : ch_view.each()) {
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

        tc.position = glm::lerp(ch.previous_translation, ch.translation, interpolation_factor);
        tc.rotation = glm::eulerAngles(glm::slerp(ch.previous_rotation, ch.Rotation, interpolation_factor));
      }
      else {
        const JPH::Vec3 position = ch.character->GetPosition();
        JPH::Vec3 rotation = ch.character->GetRotation().GetEulerAngles();

        ch.previous_translation = ch.translation;
        ch.previous_rotation = ch.Rotation;
        ch.translation = {position.GetX(), position.GetY(), position.GetZ()};
        ch.Rotation = glm::vec3(rotation.GetX(), rotation.GetY(), rotation.GetZ());
        tc.position = ch.translation;
        tc.rotation = glm::eulerAngles(ch.Rotation);
      }
      PhysicsUtils::DebugDraw(this, e);
    }
  }
}

void Scene::destroy_entity(const Entity entity) {
  OX_SCOPED_ZONE;
  entity.deparent();
  const auto children = entity.get_component<RelationshipComponent>().children;

  for (size_t i = 0; i < children.size(); i++) {
    if (const Entity child_entity = get_entity_by_uuid(children[i]))
      destroy_entity(child_entity);
  }

  entity_map.erase(entity.get_uuid());
  m_registry.destroy(entity);
}

template <typename... Component>
static void copy_component(entt::registry& dst,
                          entt::registry& src,
                          const std::unordered_map<UUID, entt::entity>& entt_map) {
  ([&] {
    auto view = src.view<Component>();
    for (auto src_entity : view) {
      entt::entity dst_entity = entt_map.at(src.get<IDComponent>(src_entity).ID);

      Component& src_component = src.get<Component>(src_entity);
      dst.emplace_or_replace<Component>(dst_entity, src_component);
    }
  }(), ...);
}

template <typename... Component>
static void copy_component(ComponentGroup<Component...>,
                          entt::registry& dst,
                          entt::registry& src,
                          const std::unordered_map<UUID, entt::entity>& entt_map) {
  copy_component<Component...>(dst, src, entt_map);
}

template <typename... Component>
static void copy_component_if_exists(Entity dst, Entity src) {
  ([&] {
    if (src.has_component<Component>())
      dst.add_or_replace_component<Component>(src.get_component<Component>());
  }(), ...);
}

template <typename... Component>
static void copy_component_if_exists(ComponentGroup<Component...>,
                                  Entity dst,
                                  Entity src) {
  copy_component_if_exists<Component...>(dst, src);
}

void Scene::duplicate_entity(Entity entity) {
  OX_SCOPED_ZONE;
  const Entity new_entity = create_entity(entity.get_name());
  copy_component_if_exists(AllComponents{}, new_entity, entity);
}

void Scene::on_runtime_start() {
  OX_SCOPED_ZONE;

  is_running = true;

  physics_frame_accumulator = 0.0f;

  // Physics
  {
    OX_SCOPED_ZONE_N("Physics Start");
    Physics::init();
    body_activation_listener_3d = new Physics3DBodyActivationListener();
    contact_listener_3d = new Physics3DContactListener(this);
    const auto physics_system = Physics::get_physics_system();
    physics_system->SetBodyActivationListener(body_activation_listener_3d);
    physics_system->SetContactListener(contact_listener_3d);

    // Rigidbodies
    {
      const auto group = m_registry.group<RigidbodyComponent>(entt::get<TransformComponent>);
      for (auto&& [e, rb, tc] : group.each()) {
        rb.previous_translation = rb.translation = tc.position;
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

    physics_system->OptimizeBroadPhase();
  }
}

void Scene::on_runtime_stop() {
  OX_SCOPED_ZONE;

  is_running = false;

  // Physics
  {
    JPH::BodyInterface& body_interface = Physics::get_physics_system()->GetBodyInterface();
    const auto rb_view = m_registry.view<RigidbodyComponent>();
    for (auto&& [e, rb] : rb_view.each()) {
      if (rb.runtime_body) {
        const auto* body = static_cast<const JPH::Body*>(rb.runtime_body);
        body_interface.RemoveBody(body->GetID());
        body_interface.DestroyBody(body->GetID());
      }
    }
    const auto ch_view = m_registry.view<CharacterControllerComponent>();
    for (auto&& [e, ch] : ch_view.each()) {
      if (ch.character) {
        body_interface.RemoveBody(ch.character->GetBodyID());
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
  Ref<Scene> new_scene = create_ref<Scene>();

  auto& src_scene_registry = other->m_registry;
  auto& dst_scene_registry = new_scene->m_registry;

  // Create entities in new scene
  const auto view = src_scene_registry.view<IDComponent, TagComponent>();
  for (const auto e : view) {
    auto [id, tag] = view.get<IDComponent, TagComponent>(e);
    const auto& name = tag.tag;
    Entity new_entity = new_scene->create_entity_with_uuid(id.ID, name);
    new_entity.get_component<TagComponent>().enabled = tag.enabled;
  }

  for (const auto e : view) {
    Entity src = {e, other.get()};
    Entity dst = new_scene->get_entity_by_uuid(view.get<IDComponent>(e).ID);
    if (Entity src_parent = src.get_parent())
      dst.set_parent(new_scene->get_entity_by_uuid(src_parent.get_uuid()));
  }

  // Copy components (except IDComponent and TagComponent)
  copy_component(AllComponents{}, dst_scene_registry, src_scene_registry, new_scene->entity_map);

  return new_scene;
}

void Scene::on_contact_added(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, const JPH::ContactSettings& settings) {
  OX_SCOPED_ZONE;
  for (const auto& system : systems)
    system->on_contact_added(this, body1, body2, manifold, settings);
}

void Scene::on_contact_persisted(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, const JPH::ContactSettings& settings) {
  OX_SCOPED_ZONE;
  for (const auto& system : systems)
    system->on_contact_persisted(this, body1, body2, manifold, settings);
}

void Scene::create_rigidbody(Entity entity, const TransformComponent& transform, RigidbodyComponent& component) const {
  OX_SCOPED_ZONE;
  if (!is_running)
    return;

  auto& body_interface = Physics::get_body_interface();
  if (component.runtime_body) {
    body_interface.DestroyBody(static_cast<JPH::Body*>(component.runtime_body)->GetID());
    component.runtime_body = nullptr;
  }

  JPH::MutableCompoundShapeSettings compound_shape_settings;
  float max_scale_component = glm::max(glm::max(transform.scale.x, transform.scale.y), transform.scale.z);

  const auto& entity_name = entity.get_component<TagComponent>().tag;

  if (entity.has_component<BoxColliderComponent>()) {
    const auto& bc = entity.get_component<BoxColliderComponent>();
    const auto* mat = new PhysicsMaterial3D(entity_name, JPH::ColorArg(255, 0, 0), bc.friction, bc.restitution);

    Vec3 scale = bc.size;
    JPH::BoxShapeSettings shape_settings({glm::abs(scale.x), glm::abs(scale.y), glm::abs(scale.z)}, 0.05f, mat);
    shape_settings.SetDensity(glm::max(0.001f, bc.density));

    compound_shape_settings.AddShape({bc.offset.x, bc.offset.y, bc.offset.z}, JPH::Quat::sIdentity(), shape_settings.Create().Get());
  }

  if (entity.has_component<SphereColliderComponent>()) {
    const auto& sc = entity.get_component<SphereColliderComponent>();
    const auto* mat = new PhysicsMaterial3D(entity_name, JPH::ColorArg(255, 0, 0), sc.friction, sc.restitution);

    float radius = 2.0f * sc.radius * max_scale_component;
    JPH::SphereShapeSettings shape_settings(glm::max(0.01f, radius), mat);
    shape_settings.SetDensity(glm::max(0.001f, sc.density));

    compound_shape_settings.AddShape({sc.offset.x, sc.offset.y, sc.offset.z}, JPH::Quat::sIdentity(), shape_settings.Create().Get());
  }

  if (entity.has_component<CapsuleColliderComponent>()) {
    const auto& cc = entity.get_component<CapsuleColliderComponent>();
    const auto* mat = new PhysicsMaterial3D(entity_name, JPH::ColorArg(255, 0, 0), cc.friction, cc.restitution);

    float radius = 2.0f * cc.radius * max_scale_component;
    JPH::CapsuleShapeSettings shape_settings(glm::max(0.01f, cc.height) * 0.5f, glm::max(0.01f, radius), mat);
    shape_settings.SetDensity(glm::max(0.001f, cc.density));

    compound_shape_settings.AddShape({cc.offset.x, cc.offset.y, cc.offset.z}, JPH::Quat::sIdentity(), shape_settings.Create().Get());
  }

  if (entity.has_component<TaperedCapsuleColliderComponent>()) {
    const auto& tcc = entity.get_component<TaperedCapsuleColliderComponent>();
    const auto* mat = new PhysicsMaterial3D(entity_name, JPH::ColorArg(255, 0, 0), tcc.friction, tcc.restitution);

    float top_radius = 2.0f * tcc.top_radius * max_scale_component;
    float bottom_radius = 2.0f * tcc.bottom_radius * max_scale_component;
    JPH::TaperedCapsuleShapeSettings shape_settings(glm::max(0.01f, tcc.height) * 0.5f, glm::max(0.01f, top_radius), glm::max(0.01f, bottom_radius), mat);
    shape_settings.SetDensity(glm::max(0.001f, tcc.density));

    compound_shape_settings.AddShape({tcc.offset.x, tcc.offset.y, tcc.offset.z}, JPH::Quat::sIdentity(), shape_settings.Create().Get());
  }

  if (entity.has_component<CylinderColliderComponent>()) {
    const auto& cc = entity.get_component<CylinderColliderComponent>();
    const auto* mat = new PhysicsMaterial3D(entity_name, JPH::ColorArg(255, 0, 0), cc.friction, cc.restitution);

    float radius = 2.0f * cc.radius * max_scale_component;
    JPH::CylinderShapeSettings shape_settings(glm::max(0.01f, cc.height) * 0.5f, glm::max(0.01f, radius), 0.05f, mat);
    shape_settings.SetDensity(glm::max(0.001f, cc.density));

    compound_shape_settings.AddShape({cc.offset.x, cc.offset.y, cc.offset.z}, JPH::Quat::sIdentity(), shape_settings.Create().Get());
  }

  // Body
  auto rotation = glm::quat(transform.rotation);

  auto layer = entity.get_component<TagComponent>().layer;
  uint8_t layer_index = 1;	// Default Layer
  auto collision_mask_it = Physics::layer_collision_mask.find(layer);
  if (collision_mask_it != Physics::layer_collision_mask.end())
    layer_index = collision_mask_it->second.index;

  JPH::BodyCreationSettings body_settings(compound_shape_settings.Create().Get(), {transform.position.x, transform.position.y, transform.position.z}, {rotation.x, rotation.y, rotation.z, rotation.w}, static_cast<JPH::EMotionType>(component.type), layer_index);

  JPH::MassProperties mass_properties;
  mass_properties.mMass = glm::max(0.01f, component.mass);
  body_settings.mMassPropertiesOverride = mass_properties;
  body_settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
  body_settings.mAllowSleeping = component.allow_sleep;
  body_settings.mLinearDamping = glm::max(0.0f, component.linear_drag);
  body_settings.mAngularDamping = glm::max(0.0f, component.angular_drag);
  body_settings.mMotionQuality = component.continuous ? JPH::EMotionQuality::LinearCast : JPH::EMotionQuality::Discrete;
  body_settings.mGravityFactor = component.gravity_scale;

  body_settings.mIsSensor = component.is_sensor;

  JPH::Body* body = body_interface.CreateBody(body_settings);

  JPH::EActivation activation = component.awake && component.type != RigidbodyComponent::BodyType::Static ? JPH::EActivation::Activate : JPH::EActivation::DontActivate;
  body_interface.AddBody(body->GetID(), activation);

  component.runtime_body = body;
}

void Scene::create_character_controller(const TransformComponent& transform, CharacterControllerComponent& component) const {
  OX_SCOPED_ZONE;
  if (!is_running)
    return;
  const auto position = JPH::Vec3(transform.position.x, transform.position.y, transform.position.z);
  const auto capsule_shape = JPH::RotatedTranslatedShapeSettings(
    JPH::Vec3(0, 0.5f * component.character_height_standing + component.character_radius_standing, 0),
    JPH::Quat::sIdentity(),
    new JPH::CapsuleShape(0.5f * component.character_height_standing, component.character_radius_standing)).Create().Get();

  // Create character
  const Ref<JPH::CharacterSettings> settings = create_ref<JPH::CharacterSettings>();
  settings->mMaxSlopeAngle = JPH::DegreesToRadians(45.0f);
  settings->mLayer = PhysicsLayers::MOVING;
  settings->mShape = capsule_shape;
  settings->mFriction = 0.0f; // For now this is not set. 
  settings->mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -component.character_radius_standing); // Accept contacts that touch the lower sphere of the capsule
  component.character = new JPH::Character(settings.get(), position, JPH::Quat::sIdentity(), 0, Physics::get_physics_system());
  component.character->AddToPhysicsSystem(JPH::EActivation::Activate);
}

void Scene::on_runtime_update(const Timestep& delta_time) {
  OX_SCOPED_ZONE;

  // Camera
  {
    OX_SCOPED_ZONE_N("Camera System");
    const auto camera_view = m_registry.view<TransformComponent, CameraComponent>();
    for (const auto entity : camera_view) {
      auto [transform, camera] = camera_view.get<TransformComponent, CameraComponent>(entity);
      camera.system->update(transform.position, transform.rotation);
      scene_renderer->get_render_pipeline()->on_register_camera(camera.system.get());
    }
  }

  scene_renderer->update();

  {
    OX_SCOPED_ZONE_N("PostOnUpdate Systems");
    for (const auto& system : systems) {
      system->pre_on_update(this, delta_time);
    }
  }
  update_physics(delta_time);
  {
    OX_SCOPED_ZONE_N("OnUpdate Systems");
    for (const auto& system : systems) {
      system->on_update(this, delta_time);
    }
  }

  // Scripting
  {
    OX_SCOPED_ZONE_N("Lua Scripting Systems");
    const auto script_view = m_registry.view<LuaScriptComponent>();
    for (auto&& [e, script_component] : script_view.each()) {
      if (script_component.lua_system)
        script_component.lua_system->on_update(this, delta_time);
    }
  }

  // Audio
  {
    OX_SCOPED_ZONE_N("Audio Systems");
    const auto listener_view = m_registry.group<AudioListenerComponent>(entt::get<TransformComponent>);
    for (auto&& [e, ac, tc] : listener_view.each()) {
      ac.listener = create_ref<AudioListener>();
      if (ac.active) {
        const glm::mat4 inverted = glm::inverse(Entity(e, this).get_world_transform());
        const glm::vec3 forward = normalize(glm::vec3(inverted[2]));
        ac.listener->SetConfig(ac.config);
        ac.listener->SetPosition(tc.position);
        ac.listener->SetDirection(-forward);
        break;
      }
    }

    const auto source_view = m_registry.group<AudioSourceComponent>(entt::get<TransformComponent>);
    for (auto&& [e, ac, tc] : source_view.each()) {
      if (ac.source) {
        const glm::mat4 inverted = glm::inverse(Entity(e, this).get_world_transform());
        const glm::vec3 forward = normalize(glm::vec3(inverted[2]));
        ac.source->SetConfig(ac.config);
        ac.source->SetPosition(tc.position);
        ac.source->SetDirection(forward);
        if (ac.config.PlayOnAwake)
          ac.source->Play();
      }
    }
  }
}

void Scene::on_imgui_render(const Timestep& delta_time) {
  OX_SCOPED_ZONE;
  for (const auto& system : systems)
    system->on_imgui_render(this, delta_time);
}

void Scene::on_editor_update(const Timestep& delta_time, Camera& camera) {
  OX_SCOPED_ZONE;
  scene_renderer->get_render_pipeline()->on_register_camera(&camera);
  scene_renderer->update();

  const auto rb_view = m_registry.view<RigidbodyComponent>();
  for (auto&& [e, rb] : rb_view.each()) {
    PhysicsUtils::DebugDraw(this, e);
  }
  const auto ch_view = m_registry.view<CharacterControllerComponent>();
  for (auto&& [e, ch] : ch_view.each()) {
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
void Scene::on_component_added<MeshComponent>(Entity entity, MeshComponent& component) {
  entity.add_component_internal<MaterialComponent>();
  if (!component.original_mesh->animations.empty()) {
    auto& animation_component = entity.add_component_internal<AnimationComponent>();
    animation_component.animations = component.original_mesh->animations;
  }
}

template <>
void Scene::on_component_added<SkyLightComponent>(Entity entity, SkyLightComponent& component) { }

template <>
void Scene::on_component_added<MaterialComponent>(Entity entity, MaterialComponent& component) {
  if (component.materials.empty()) {
    if (entity.has_component<MeshComponent>())
      component.materials = entity.get_component<MeshComponent>().original_mesh->get_materials_as_ref();
  }
}

template <>
void Scene::on_component_added<AnimationComponent>(Entity entity, AnimationComponent& component) { }

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

template <>
void Scene::on_component_added<LuaScriptComponent>(Entity entity, LuaScriptComponent& component) { }
}
