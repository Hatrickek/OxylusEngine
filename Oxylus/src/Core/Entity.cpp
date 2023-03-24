#include "oxpch.h"
#include "Entity.h"
#include "Scene/Scene.h"

namespace Oxylus {
  Scene* m_Scene = nullptr;

  Entity::Entity(entt::entity handle, Scene* scene) : m_EntityHandle(handle), m_Scene(scene) { }
}
