#include "entity.h"

namespace FlatEngine {
	Scene* m_Scene = nullptr;

	Entity::Entity(entt::entity handle, Scene* scene)
		: m_EntityHandle(handle), m_Scene(scene) {
		
	}
}