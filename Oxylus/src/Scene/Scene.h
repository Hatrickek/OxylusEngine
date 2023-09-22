#pragma once

#include "EntitySerializer.h"
#include "SceneRenderer.h"
#include "Core/Components.h"

#include "Core/UUID.h"
#include "Core/Systems/System.h"
#include "Jolt/Physics/PhysicsSystem.h"
#include "Jolt/Physics/Body/BodyInterface.h"
#include "Physics/PhyiscsInterfaces.h"
#include "Render/Mesh.h"
#include <entt/entity/registry.hpp>

namespace Oxylus {
class Entity;

class Scene {
public:
  Scene();

  Scene(std::string name);

  ~Scene();
  Scene(const Scene&);

  Entity CreateEntity(const std::string& name);
  Entity CreateEntityWithUUID(UUID uuid, const std::string& name = std::string());
  void CreateEntityWithMesh(const Ref<Mesh>& meshAsset);

  template <typename T, typename... Args>
  Scene* AddSystem(Args&&... args) {
    m_Systems.emplace_back(CreateScope<T>(std::forward<Args>(args)...));
    return this;
  }

  void DestroyEntity(Entity entity);
  void DuplicateEntity(Entity entity);

  void OnRuntimeStart();
  void OnRuntimeStop();

  void OnRuntimeUpdate(float deltaTime);
  void OnEditorUpdate(float deltaTime, Camera& camera);

  void OnImGuiRender(float deltaTime);

  Entity FindEntity(const std::string_view& name);
  bool HasEntity(UUID uuid) const;
  static Ref<Scene> Copy(const Ref<Scene>& other);

  // Physics interfaces
  void OnContactAdded(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, const JPH::ContactSettings& settings);
  void OnContactPersisted(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, JPH::ContactSettings& settings);

  Entity GetEntityByUUID(UUID uuid);
  Ref<SceneRenderer> GetRenderer() { return m_SceneRenderer; }

  std::string SceneName = "Untitled";
  entt::registry m_Registry;
  std::unordered_map<UUID, entt::entity> m_EntityMap;

private:
  void Init();

  // Physics
  void UpdatePhysics(Timestep deltaTime);
  void CreateRigidbody(Entity entity, const TransformComponent& transform, RigidbodyComponent& component) const;
  void CreateCharacterController(const TransformComponent& transform, CharacterControllerComponent& component) const;

  template <typename T>
  void OnComponentAdded(Entity entity, T& component);

  void IterateOverMeshNode(const Ref<Mesh>& mesh, const std::vector<Mesh::Node*>& node, Entity parent);

  bool m_IsRunning = false;

  // Renderer
  Ref<SceneRenderer> m_SceneRenderer;

  // Systems
  std::vector<Scope<System>> m_Systems;

  // Physics
  Physics3DContactListener* m_ContactListener3D = nullptr;
  Physics3DBodyActivationListener* m_BodyActivationListener3D = nullptr;
  float m_PhysicsFrameAccumulator = 0.0f;

  friend class Entity;
  friend class SceneSerializer;
  friend class SceneHPanel;
};
}
