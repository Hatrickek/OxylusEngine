#include "Entity.h"
#include "Scene.h"

namespace Ox {
Entity::Entity(entt::entity handle, Scene* scene) : entity_handle(handle), scene(scene) { }
}
