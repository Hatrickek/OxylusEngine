#include "Entity.h"
#include "Scene/Scene.h"

namespace Oxylus {
Entity::Entity(entt::entity handle, Scene* scene) : m_EntityHandle(handle), m_Scene(scene) { }
}
