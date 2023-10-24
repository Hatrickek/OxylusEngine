#pragma once

#include "Core/UUID.h"

#include "Core/YamlHelpers.h"

namespace Oxylus {
class Entity;

class Scene;

class EntitySerializer {
public:
  static void SerializeEntity(Scene* scene, ryml::NodeRef& entities, Entity entity);

  static UUID DeserializeEntity(ryml::ConstNodeRef entityNode, Scene* scene, bool preserveUUID);

  static void SerializeEntityAsPrefab(const char* filepath, Entity entity);

  static Entity DeserializeEntityAsPrefab(const char* filepath, Scene* scene);
};
}
