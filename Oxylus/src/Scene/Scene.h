#pragma once

#include "EntitySerializer.h"
#include "SceneRenderer.h"
#include "Assets/Assets.h"

#include "Core/UUID.h"
#include "Core/Systems/System.h"
#include "entt/entt.hpp"
#include "Jolt/Core/JobSystemThreadPool.h"
#include "Jolt/Physics/PhysicsSystem.h"
#include "Jolt/Physics/Body/BodyInterface.h"
#include "Render/Mesh.h"
#include "Render/Camera.h"

namespace Oxylus {
  class Entity;

  class Scene {
  public:
    Scene(bool initPhysics = true);

    Scene(std::string name);

    ~Scene();

    Scene(const Scene&);

    Entity CreateEntity(const std::string& name);
    Entity CreateEntityWithUUID(UUID uuid, const std::string& name = std::string());
    void CreateEntityWithMesh(const Asset<Mesh>& meshAsset);

    template <typename T, typename... Args>
    Scene* AddSystem(Args&&... args) {
      m_Systems.emplace_back(CreateScope<T>(std::forward<Args>(args)...));
      return this;
    }

    void DestroyEntity(Entity entity);
    void DuplicateEntity(Entity entity);
    void OnPlay();
    void OnStop();
    void OnUpdate(float deltaTime);
    void OnEditorUpdate(float deltaTime, Camera& camera) const;
    void RenderScene() const;
    void UpdateSystems();
    Entity FindEntity(const std::string_view& name);
    bool HasEntity(UUID uuid) const;
    Entity GetEntityByUUID(UUID uuid);
    static Ref<Scene> Copy(const Ref<Scene>& other);
    SceneRenderer& GetRenderer() { return m_SceneRenderer; }

    // Physics
    JPH::BodyInterface* GetBodyInterface() const { return m_BodyInterface; }

    std::string SceneName = "Untitled";
    entt::registry m_Registry;
    std::unordered_map<UUID, entt::entity> m_EntityMap;

  private:
    void Init();
    void InitPhysics();
    void UpdatePhysics();
    template <typename T>
    void OnComponentAdded(Entity entity, T& component);

    void IterateOverMeshNode(const Ref<Mesh>& mesh, const std::vector<Mesh::Node*>& node, Entity parent);

    // Renderer
    SceneRenderer m_SceneRenderer;

    // Systems
    std::vector<Scope<System>> m_Systems;

    // Physics
    JPH::BodyInterface* m_BodyInterface = nullptr;
    Ref<JPH::PhysicsSystem> m_PhysicsSystem = nullptr;
    Ref<JPH::JobSystemThreadPool> m_JobSystem = nullptr;
   
    friend class Entity;
    friend class SceneSerializer;
    friend class SceneHPanel;
  };
}
