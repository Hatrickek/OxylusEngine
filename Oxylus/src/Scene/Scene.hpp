#pragma once

#include <ankerl/unordered_dense.h>

#include "Components.hpp"
#include "EntitySerializer.hpp"

#include <entt/entity/registry.hpp>
#include "Core/Systems/System.hpp"
#include "Core/UUID.hpp"
#include "Physics/PhysicsInterfaces.hpp"
#include "Render/Mesh.h"

namespace ox {
class RenderPipeline;
class SceneRenderer;

class Scene {
public:
  std::string scene_name = "Untitled";
  entt::registry registry;
  // TODO: We are keeping this only for serializing relationship of entities.
  // Check how we can do that with entt id's instead.
  ankerl::unordered_dense::map<UUID, Entity> entity_map;

  Scene();
  Scene(std::string name);
  Scene(const Shared<RenderPipeline>& render_pipeline);
  Scene(const Scene&);

  ~Scene();

  Entity create_entity(const std::string& name = "New Entity");
  Entity create_entity_with_uuid(UUID uuid, const std::string& name = std::string());

  void iterate_mesh_node(const Shared<Mesh>& mesh, std::vector<Entity>& node_entities, Entity parent_entity, const Mesh::Node* node);
  Entity load_mesh(const Shared<Mesh>& mesh);

  template <typename T, typename... Args>
  Scene* add_system(Args&&... args) {
    systems.emplace_back(create_unique<T>(std::forward<Args>(args)...));
    return this;
  }

  void destroy_entity(Entity entity);
  void duplicate_children(Entity entity);
  Entity duplicate_entity(Entity entity);

  void on_runtime_start();
  void on_runtime_stop();

  bool is_running() const { return running; }

  void on_runtime_update(const Timestep& delta_time);
  void on_editor_update(const Timestep& delta_time, Camera& camera);

  void on_imgui_render(const Timestep& delta_time);

  Entity find_entity(const std::string_view& name);
  bool has_entity(UUID uuid) const;
  static Shared<Scene> copy(const Shared<Scene>& src_scene);

  // Physics interfaces
  void on_contact_added(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, const JPH::ContactSettings& settings);
  void
  on_contact_persisted(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, const JPH::ContactSettings& settings);

  Entity get_entity_by_uuid(UUID uuid);

  // Renderer
  Shared<SceneRenderer> get_renderer() { return scene_renderer; }

  entt::registry& get_registry() { return registry; }

private:
  bool running = false;

  // Renderer
  Shared<SceneRenderer> scene_renderer;

  // Systems
  std::vector<Unique<System>> systems;

  // Physics
  Physics3DContactListener* contact_listener_3d = nullptr;
  Physics3DBodyActivationListener* body_activation_listener_3d = nullptr;
  float physics_frame_accumulator = 0.0f;

  void init(const Shared<RenderPipeline>& render_pipeline = nullptr);

  void rigidbody_component_ctor(entt::registry& reg, Entity entity);
  void collider_component_ctor(entt::registry& reg, Entity entity);
  void character_controller_component_ctor(entt::registry& reg, Entity entity) const;

  // Physics
  void update_physics(const Timestep& delta_time);
  void create_rigidbody(Entity ent, const TransformComponent& transform, RigidbodyComponent& component);
  void create_character_controller(const TransformComponent& transform, CharacterControllerComponent& component) const;

  friend class SceneSerializer;
  friend class SceneHPanel;
};
} // namespace ox
