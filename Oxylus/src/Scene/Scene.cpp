#include "src/oxpch.h"
#include "Scene.h"

#include "Core/Entity.h"
#include "Core/ScriptableEntity.h"
#include "Render/Camera.h"
#include "Render/Vulkan/VulkanRenderer.h"
#include "Utils/Log.h"
#include "Utils/Profiler.h"
#include "Utils/Timestep.h"

#include <glm/glm.hpp>

#include <unordered_map>

namespace Oxylus {
  Scene::Scene() {
    m_SceneRenderer.Init(*this);
    for (const auto& system : m_Systems) {
      system->OnInit();
    }
  }

  Scene::Scene(std::string name) : SceneName(std::move(name)) { }

  Scene::~Scene() = default;

  Scene::Scene(const Scene& scene) {
    const auto& reg = scene.m_Registry;
    this->SceneName = scene.SceneName;
    this->m_Registry.assign(reg.data(), reg.data() + reg.size(), reg.released());
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
    uint32_t nodeIndex = 0;
    for (const auto& node : mesh->Nodes) {
      Entity entity = CreateEntity(node->Name);
      if (node->ContainsMesh) {
        entity.AddComponentI<MeshRendererComponent>(mesh).SubmesIndex = nodeIndex;
        entity.GetComponent<MaterialComponent>().Materials = mesh->GetMaterialsAsRef();
      }
      IterateOverMeshNode(mesh, node->Children, entity);
      ++nodeIndex;
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

    // Copy components (except IDComponent and TagComponent)
    CopyComponent(AllComponents{}, dstSceneRegistry, srcSceneRegistry, newScene->m_EntityMap);

    return newScene;
  }

  void Scene::RenderScene() const {
    m_SceneRenderer.Render();
  }

  void Scene::UpdateSystems() {
    ZoneScopedN("Update Systems");
    for (const auto& system : m_Systems) {
      system->OnUpdate(this);
    }
  }

  void Scene::OnUpdate(float deltaTime) {
    ZoneScoped;

    UpdateSystems();

    RenderScene();

    Physics::Update(deltaTime);

    //Camera
    {
      ZoneScopedN("Camera System");
      const auto group = m_Registry.view<TransformComponent, CameraComponent>();
      for (const auto entity : group) {
        auto [transform, camera] = group.get<TransformComponent, CameraComponent>(entity);
        camera.System->Update(transform.Translation, transform.Rotation);
        VulkanRenderer::SetCamera(*camera.System);
      }
    }

    //Scripts
    {
      ZoneScopedN("Scripts System");
      m_Registry.view<NativeScriptComponent>().each([this, deltaTime](auto entity, auto& nsc) {
        if (!nsc.Instance) {
          nsc.Instance = nsc.InstantiateScript();
          nsc.Instance->m_Entity = Entity{entity, this};
          nsc.Instance->OnCreate();
        }

        nsc.Instance->OnUpdate(deltaTime);
      });
    }

    //Physics
    {
      ZoneScopedN("Physics System");
      //Rigidbody
      /*{
        const auto group = m_Registry.view<TransformComponent, RigidBodyComponent>();
        for (const auto entity : group) {
          auto [transform, rb] = group.get<TransformComponent, RigidBodyComponent>(entity);
          rb.Rigidbody->OnUpdate(transform.Translation, transform.Rotation);
        }
      }
      //Box collider
      {
        const auto group = m_Registry.view<TransformComponent, RigidBodyComponent, BoxColliderComponent>();
        for (const auto entity : group) {
          auto [transform, rb, boxCollider] = group.get<
            TransformComponent, RigidBodyComponent, BoxColliderComponent>(entity);
          boxCollider.size = transform.Scale;
          rb.Rigidbody->SetGeometry(physx::PxBoxGeometry(boxCollider.size.x, boxCollider.size.y, boxCollider.size.z));
        }
      }*/
    }

    //Audio
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

  void Scene::OnEditorUpdate(float deltaTime, Camera& camera) {
    RenderScene();

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

  template <>
  void Scene::OnComponentAdded<NativeScriptComponent>(Entity entity, NativeScriptComponent& component) { }
}
