#pragma once

#include "Utils/Log.h"

#include "Scene/Scene.h"
#include "Components.h"

namespace Oxylus {
class Entity {
public:
  Entity() = default;

  Entity(entt::entity handle, Scene* scene);

  Entity(const Entity& other) = default;

  //Add component for Internal components which calls OnComponentAdded for them.
  template <typename T, typename... Args>
  T& AddComponentI(Args&&... args) {
    if (HasComponent<T>()) {
      OX_CORE_ERROR("Entity already has {0}!", typeid(T).name());
    }

    T& component = m_Scene->m_Registry.emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
    m_Scene->OnComponentAdded<T>(*this, component);
    return component;
  }

  //Add component for exposing the raw ECS system for use.
  template <typename T, typename... Args>
  T& AddComponent(Args&&... args) {
    if (HasComponent<T>()) {
      OX_CORE_ERROR("Entity already has {0}!", typeid(T).name());
    }

    T& component = m_Scene->m_Registry.emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
    return component;
  }

  template <typename T>
  T& GetComponent() const {
    if (!HasComponent<T>()) {
      OX_CORE_ERROR("Entity doesn't have {0}!", typeid(T).name());
    }
    return m_Scene->m_Registry.get<T>(m_EntityHandle);
  }

  template <typename T>
  bool HasComponent() const {
    return m_Scene->m_Registry.all_of<T>(m_EntityHandle);
  }

  template <typename T>
  void RemoveComponent() const {
    if (!HasComponent<T>()) {
      OX_CORE_ERROR("Entity does not have {0} to remove!", typeid(T).name());
    }
    m_Scene->m_Registry.remove<T>(m_EntityHandle);
  }

  template <typename T, typename... Args>
  T& AddOrReplaceComponent(Args&&... args) {
    T& component = m_Scene->m_Registry.emplace_or_replace<T>(m_EntityHandle, std::forward<Args>(args)...);
    m_Scene->OnComponentAdded<T>(*this, component);
    return component;
  }

  RelationshipComponent& GetRelationship() const { return GetComponent<RelationshipComponent>(); }
  UUID GetUUID() const { return GetComponent<IDComponent>().ID; }
  const std::string& GetName() const { return GetComponent<TagComponent>().Tag; }
  TransformComponent& GetTransform() const { return GetComponent<TransformComponent>(); }

  Entity GetParent() const {
    if (!m_Scene)
      return {};

    const auto& rc = GetComponent<RelationshipComponent>();
    return rc.Parent != 0 ? m_Scene->GetEntityByUUID(rc.Parent) : Entity{};
  }

  Entity GetChild(uint32_t index = 0) const {
    const auto& rc = GetComponent<RelationshipComponent>();
    return GetScene()->GetEntityByUUID(rc.Children[index]);
  }

  std::vector<Entity> GetAllChildren() const {
    std::vector<Entity> entities;

    const std::vector<UUID> children = GetComponent<RelationshipComponent>().Children;
    for (const auto& child : children) {
      Entity entity = GetScene()->GetEntityByUUID(child);
      entities.push_back(entity);
      GetAllChildren();
    }

    return entities;
  }

  static void GetAllChildren(Entity parent, std::vector<Entity>& outEntities) {
    const std::vector<UUID> children = parent.GetComponent<RelationshipComponent>().Children;
    for (const auto& child : children) {
      Entity entity = parent.GetScene()->GetEntityByUUID(child);
      outEntities.push_back(entity);
      GetAllChildren(entity, outEntities);
    }
  }

  Entity SetParent(Entity parent) const {
    OX_CORE_ASSERT(m_Scene->m_EntityMap.contains(parent.GetUUID()), "Parent is not in the same scene as entity")
    Deparent();

    auto& [Parent, Children] = GetComponent<RelationshipComponent>();
    Parent = parent.GetUUID();
    parent.GetRelationship().Children.emplace_back(GetUUID());

    return *this;
  }

  void Deparent() const {
    auto& transform = GetRelationship();
    const UUID uuid = GetUUID();
    const Entity parentEntity = GetParent();

    if (!parentEntity)
      return;

    auto& parent = parentEntity.GetRelationship();
    for (auto it = parent.Children.begin(); it != parent.Children.end(); ++it) {
      if (*it == uuid) {
        parent.Children.erase(it);
        break;
      }
    }
    transform.Parent = 0;
  }


  glm::mat4 GetWorldTransform() const {
    const auto& transform = GetTransform();
    const auto& rc = GetRelationship();
    const Entity parent = m_Scene->GetEntityByUUID(rc.Parent);
    const glm::mat4 parentTransform = parent ? parent.GetWorldTransform() : glm::mat4(1.0f);
    return parentTransform * glm::translate(glm::mat4(1.0f), transform.Translation) *
           glm::toMat4(glm::quat(transform.Rotation)) * glm::scale(glm::mat4(1.0f), transform.Scale);
  }

  glm::mat4 GetLocalTransform() const {
    const auto& transform = GetTransform();
    return glm::translate(glm::mat4(1.0f), transform.Translation) * glm::toMat4(glm::quat(transform.Rotation)) *
           glm::scale(glm::mat4(1.0f), transform.Scale);
  }

  Scene* GetScene() const { return m_Scene; }

  operator bool() const {
    return m_EntityHandle != entt::null;
  }

  operator entt::entity() const {
    return m_EntityHandle;
  }

  operator uint32_t() const {
    return (uint32_t)m_EntityHandle;
  }

  bool operator==(const Entity& other) const {
    return m_EntityHandle == other.m_EntityHandle && m_Scene == other.m_Scene;
  }

  bool operator!=(const Entity& other) const {
    return !(*this == other);
  }

private:
  entt::entity m_EntityHandle{entt::null};
  Scene* m_Scene = nullptr;
};
}
