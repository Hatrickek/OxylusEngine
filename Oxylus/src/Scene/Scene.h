#pragma once

#include "EntitySerializer.h"
#include "SceneRenderer.h"
#include "Core/Components.h"

#include "Core/UUID.h"
#include "Core/Systems/System.h"
#include "Jolt/Physics/PhysicsSystem.h"
#include "Jolt/Physics/Body/BodyInterface.h"
#include "Physics/PhysicsInterfaces.h"
#include "Render/Mesh.h"
#include <entt/entity/registry.hpp>

#include "Core/Keycodes.h"

namespace Oxylus {
class Entity;

class Scene {
public:
  std::string scene_name = "Untitled";
  entt::registry m_registry;
  std::unordered_map<UUID, entt::entity> entity_map;

  Scene();

  Scene(std::string name);

  ~Scene();
  Scene(const Scene&);

  Entity create_entity(const std::string& name = "New Entity");
  Entity create_entity_with_uuid(UUID uuid, const std::string& name = std::string());
  void create_entity_with_mesh(const Ref<Mesh>& mesh_asset);

  template <typename T, typename... Args>
  Scene* add_system(Args&&... args) {
    systems.emplace_back(create_scope<T>(std::forward<Args>(args)...));
    return this;
  }

  void destroy_entity(Entity entity);
  void duplicate_entity(Entity entity);

  void on_runtime_start();
  void on_runtime_stop();

  void on_runtime_update(const Timestep& delta_time);
  void on_editor_update(const Timestep& delta_time, Camera& camera);

  void on_imgui_render(const Timestep& delta_time);

  Entity find_entity(const std::string_view& name);
  bool has_entity(UUID uuid) const;
  static Ref<Scene> copy(const Ref<Scene>& other);

  // Physics interfaces
  void on_contact_added(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, const JPH::ContactSettings& settings);
  void on_contact_persisted(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, JPH::ContactSettings& settings);

  // Input interfaces
  void on_key_pressed(KeyCode key) const;
  void on_key_released(KeyCode key) const;

  Entity get_entity_by_uuid(UUID uuid);
  Ref<SceneRenderer> get_renderer() { return scene_renderer; }

  entt::registry& get_registry() { return m_registry;}

private:
  bool is_running = false;


  // Renderer
  Ref<SceneRenderer> scene_renderer;

  // Systems
  std::vector<Scope<System>> systems;

  // Physics
  Physics3DContactListener* contact_listener_3d = nullptr;
  Physics3DBodyActivationListener* body_activation_listener_3d = nullptr;
  float physics_frame_accumulator = 0.0f;

  void init();

  // Physics
  void update_physics(const Timestep& delta_time);
  void create_rigidbody(Entity entity, const TransformComponent& transform, RigidbodyComponent& component) const;
  void create_character_controller(const TransformComponent& transform, CharacterControllerComponent& component) const;

  template <typename T>
  void on_component_added(Entity entity, T& component);

  void iterate_over_mesh_node(const Ref<Mesh>& mesh, const std::vector<Mesh::Node*>& node, Entity parent);


  friend class Entity;
  friend class SceneSerializer;
  friend class SceneHPanel;
};
}
