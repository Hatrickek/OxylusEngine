#pragma once

#include "Utils/Log.h"

#include "Scene/Scene.h"
#include "Components.h"

namespace Oxylus {
class Entity {
public:
  Entity() = default;
  ~Entity() = default;

  Entity(entt::entity handle, Scene* scene);

  //Add component for Internal components which calls OnComponentAdded for them.
  template <typename T, typename... Args>
  T& add_component_internal(Args&&... args) {
    OX_SCOPED_ZONE;
    if (has_component<T>()) {
      OX_CORE_ERROR("Entity already has {0}!", typeid(T).name());
    }

    T& component = m_Scene->m_registry.emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
    m_Scene->on_component_added<T>(*this, component);
    return component;
  }

  //Add component for exposing the raw ECS system for use.
  template <typename T, typename... Args>
  T& add_component(Args&&... args) {
    OX_SCOPED_ZONE;
    if (has_component<T>()) {
      OX_CORE_ERROR("Entity already has {0}!", typeid(T).name());
    }

    T& component = m_Scene->m_registry.emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
    return component;
  }

  template <typename T>
  T& get_component() const {
    OX_SCOPED_ZONE;
    if (!has_component<T>()) {
      OX_CORE_ERROR("Entity doesn't have {0}!", typeid(T).name());
    }
    return m_Scene->m_registry.get<T>(m_EntityHandle);
  }

  template <typename T>
  bool has_component() const {
    OX_SCOPED_ZONE;
    return m_Scene->m_registry.all_of<T>(m_EntityHandle);
  }

  template <typename T>
  void remove_component() const {
    OX_SCOPED_ZONE;
    if (!has_component<T>()) {
      OX_CORE_ERROR("Entity does not have {0} to remove!", typeid(T).name());
    }
    m_Scene->m_registry.remove<T>(m_EntityHandle);
  }

  template <typename T, typename... Args>
  T& add_or_replace_component(Args&&... args) {
    OX_SCOPED_ZONE;
    T& component = m_Scene->m_registry.emplace_or_replace<T>(m_EntityHandle, std::forward<Args>(args)...);
    m_Scene->on_component_added<T>(*this, component);
    return component;
  }

  template <typename T, typename... Args>
  T& get_or_add_component(Args&&... args) {
    OX_SCOPED_ZONE;
    return m_Scene->m_registry.get_or_emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
  }

  template <typename T>
  T* try_get_component() {
    OX_SCOPED_ZONE;
    return m_Scene->m_registry.try_get<T>(m_EntityHandle);
  }

  RelationshipComponent& get_relationship() const { return get_component<RelationshipComponent>(); }
  UUID get_uuid() const { return get_component<IDComponent>().ID; }
  const std::string& get_name() const { return get_component<TagComponent>().tag; }
  TransformComponent& get_transform() const { return get_component<TransformComponent>(); }

  Entity get_parent() const {
    if (!m_Scene)
      return {};

    const auto& rc = get_component<RelationshipComponent>();
    return rc.parent != 0 ? m_Scene->get_entity_by_uuid(rc.parent) : Entity{};
  }

  Entity get_child(uint32_t index = 0) const {
    const auto& rc = get_component<RelationshipComponent>();
    return get_scene()->get_entity_by_uuid(rc.children[index]);
  }

  std::vector<Entity> get_all_children() const {
    std::vector<Entity> entities;

    const std::vector<UUID> children = get_component<RelationshipComponent>().children;
    for (const auto& child : children) {
      Entity entity = get_scene()->get_entity_by_uuid(child);
      entities.push_back(entity);
      get_all_children();
    }

    return entities;
  }

  static void get_all_children(Entity parent, std::vector<Entity>& out_entities) {
    const std::vector<UUID> children = parent.get_component<RelationshipComponent>().children;
    for (const auto& child : children) {
      Entity entity = parent.get_scene()->get_entity_by_uuid(child);
      out_entities.push_back(entity);
      get_all_children(entity, out_entities);
    }
  }

  Entity set_parent(Entity parent) const {
    OX_CORE_ASSERT(m_Scene->entity_map.contains(parent.get_uuid()), "Parent is not in the same scene as entity")
    deparent();

    auto& [Parent, Children] = get_component<RelationshipComponent>();
    Parent = parent.get_uuid();
    parent.get_relationship().children.emplace_back(get_uuid());

    return *this;
  }

  void deparent() const {
    auto& transform = get_relationship();
    const UUID uuid = get_uuid();
    const Entity parent_entity = get_parent();

    if (!parent_entity)
      return;

    auto& parent = parent_entity.get_relationship();
    for (auto it = parent.children.begin(); it != parent.children.end(); ++it) {
      if (*it == uuid) {
        parent.children.erase(it);
        break;
      }
    }
    transform.parent = 0;
  }


  glm::mat4 get_world_transform() const {
    OX_SCOPED_ZONE;
    const auto& transform = get_transform();
    const auto& rc = get_relationship();
    const Entity parent = m_Scene->get_entity_by_uuid(rc.parent);
    const glm::mat4 parent_transform = parent ? parent.get_world_transform() : glm::mat4(1.0f);
    return parent_transform * glm::translate(glm::mat4(1.0f), transform.position) *
           glm::toMat4(glm::quat(transform.rotation)) * glm::scale(glm::mat4(1.0f), transform.scale);
  }

  glm::mat4 get_local_transform() const {
    OX_SCOPED_ZONE;
    const auto& transform = get_transform();
    return glm::translate(glm::mat4(1.0f), transform.position) * glm::toMat4(glm::quat(transform.rotation)) *
           glm::scale(glm::mat4(1.0f), transform.scale);
  }

  Scene* get_scene() const { return m_Scene; }

  operator entt::entity() const {
    return m_EntityHandle;
  }

  operator uint32_t() const {
    return (uint32_t)m_EntityHandle;
  }

  operator bool() const {
    return m_EntityHandle != entt::null && m_Scene;
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
