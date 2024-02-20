#pragma once

#include "Utils/Toml.h"
#include "Core/UUID.h"
#include "Scene/Entity.h"

namespace Ox {
class Scene;

class EntitySerializer {
public:
  static void serialize_entity(toml::array* entities, Scene* scene, Entity entity);

  static UUID deserialize_entity(toml::array* entity_arr, Scene* scene, bool preserve_uuid);

  static void serialize_entity_as_prefab(const char* filepath, Entity entity);

  static Entity deserialize_entity_as_prefab(const char* filepath, Scene* scene);
};
}
