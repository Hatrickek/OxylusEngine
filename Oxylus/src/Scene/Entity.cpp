#include "Entity.h"
#include "Scene.h"

namespace Oxylus {
Entity::Entity(entt::entity handle, Scene* scene) : entity_handle(handle), scene(scene) { }
}
