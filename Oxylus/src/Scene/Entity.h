#pragma once

#include "Utils/Log.h"

#include "Scene.h"
#include "Components.h"

namespace Ox {
class Entity {
public:
  Entity() = default;
  ~Entity() = default;

  Entity(entt::entity handle, Scene* scene);

  // Add component for Internal components which calls OnComponentAdded for them.
  template <typename T, typename... Args>
  T& add_component_internal(Args&&... args) {
    OX_SCOPED_ZONE;
    if (has_component<T>()) {
      OX_CORE_ERROR("Entity already has {0}!", typeid(T).name());
    }

    T& component = scene->m_registry.emplace<T>(entity_handle, std::forward<Args>(args)...);
    scene->on_component_added<T>(*this, component);
    return component;
  }

  template <typename T, typename... Args>
  T& add_component(Args&&... args) {
    OX_SCOPED_ZONE;
    if (has_component<T>()) {
      OX_CORE_ERROR("Entity already has {0}!", typeid(T).name());
    }

    T& component = scene->m_registry.emplace<T>(entity_handle, std::forward<Args>(args)...);
    return component;
  }

  template <typename T>
  T& get_component() const {
    OX_SCOPED_ZONE;
    if (!has_component<T>()) {
      OX_CORE_ERROR("Entity doesn't have {0}!", typeid(T).name());
    }
    return scene->m_registry.get<T>(entity_handle);
  }

  template <typename T>
  bool has_component() const {
    OX_SCOPED_ZONE;
    return scene->m_registry.all_of<T>(entity_handle);
  }

  template <typename T>
  void remove_component() const {
    OX_SCOPED_ZONE;
    if (!has_component<T>()) {
      OX_CORE_ERROR("Entity does not have {0} to remove!", typeid(T).name());
    }
    scene->m_registry.remove<T>(entity_handle);
  }

  template <typename T, typename... Args>
  T& add_or_replace_component(Args&&... args) {
    OX_SCOPED_ZONE;
    T& component = scene->m_registry.emplace_or_replace<T>(entity_handle, std::forward<Args>(args)...);
    scene->on_component_added<T>(*this, component);
    return component;
  }

  template <typename T, typename... Args>
  T& get_or_add_component(Args&&... args) {
    OX_SCOPED_ZONE;
    return scene->m_registry.get_or_emplace<T>(entity_handle, std::forward<Args>(args)...);
  }

  template <typename T>
  T* try_get_component() {
    OX_SCOPED_ZONE;
    return scene->m_registry.try_get<T>(entity_handle);
  }

  RelationshipComponent& get_relationship() const { return get_component<RelationshipComponent>(); }
  UUID get_uuid() const { return get_component<IDComponent>().ID; }
  const std::string& get_name() const { return get_component<TagComponent>().tag; }
  TransformComponent& get_transform() const { return get_component<TransformComponent>(); }

  Entity get_parent() const {
    if (!scene)
      return {};

    const auto& rc = get_component<RelationshipComponent>();
    return rc.parent != 0 ? scene->get_entity_by_uuid(rc.parent) : Entity{};
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
      get_all_children(entity, entities);
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
    OX_CORE_ASSERT(scene->entity_map.contains(parent.get_uuid()), "Parent is not in the same scene as entity")
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


  Mat4 get_world_transform() const {
    OX_SCOPED_ZONE;
    const auto& transform = get_transform();
    const auto& rc = get_relationship();
    const Entity parent = scene->get_entity_by_uuid(rc.parent);
    const Mat4 parent_transform = parent ? parent.get_world_transform() : Mat4(1.0f);
    return parent_transform * glm::translate(Mat4(1.0f), transform.position) *
           glm::toMat4(glm::quat(transform.rotation)) * glm::scale(Mat4(1.0f), transform.scale);
  }

  Mat4 get_local_transform() const {
    OX_SCOPED_ZONE;
    const auto& transform = get_transform();
    return glm::translate(Mat4(1.0f), transform.position) * glm::toMat4(glm::quat(transform.rotation)) *
           glm::scale(Mat4(1.0f), transform.scale);
  }

  Scene* get_scene() const { return scene; }

  operator entt::entity() const {
    return entity_handle;
  }

  operator uint32_t() const {
    return (uint32_t)entity_handle;
  }

  operator bool() const {
    return entity_handle != entt::null && scene;
  }

  bool operator==(const Entity& other) const {
    return entity_handle == other.entity_handle && scene == other.scene;
  }

  bool operator!=(const Entity& other) const {
    return !(*this == other);
  }

private:
  entt::entity entity_handle{entt::null};
  Scene* scene = nullptr;
};
}
