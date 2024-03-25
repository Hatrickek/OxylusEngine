#pragma once

#include <entt/entity/registry.hpp>

#include "Core/Types.hpp"
#include "Core/UUID.hpp"

namespace ox {
using Entity = entt::entity;

class Scene;

/**
 * \brief Utility entity class
 */
class EUtil {
public:
  static const UUID& get_uuid(const entt::registry& reg, entt::entity ent);
  static const std::string& get_name(const entt::registry& reg, entt::entity ent);

  static entt::entity get_parent(Scene* scene, entt::entity entity);
  static entt::entity get_child(Scene* scene, entt::entity entity, uint32_t index);
  static void get_all_children(Scene* scene, entt::entity parent, std::vector<entt::entity>& out_entities);
  static void set_parent(Scene* scene, entt::entity entity, entt::entity parent);
  static void deparent(Scene* scene, entt::entity entity);
  static Mat4 get_world_transform(Scene* scene, Entity entity);
  static Mat4 get_local_transform(Scene* scene, Entity entity);
};
}
