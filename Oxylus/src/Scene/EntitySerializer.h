#pragma once

#include "Core/UUID.h"

#include "Core/YamlHelpers.h"

namespace Oxylus {
class Entity;

class Scene;

class EntitySerializer {
public:
  static void serialize_entity(Scene* scene, ryml::NodeRef& entities, Entity entity);

  static UUID deserialize_entity(ryml::ConstNodeRef entity_node, Scene* scene, bool preserve_uuid);

  static void serialize_entity_as_prefab(const char* filepath, Entity entity);

  static Entity deserialize_entity_as_prefab(const char* filepath, Scene* scene);
};
}
