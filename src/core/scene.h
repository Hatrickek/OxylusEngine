#pragma once
#include "entt/entt.hpp"
#include "components.h"
namespace FlatEngine{

	class Entity;

	class Scene {
	public:
		Scene();
		~Scene();
		Entity CreateEntity(const std::string& name);
		void DestroyEntity(Entity entity);

		void OnUpdate(float deltaTime);

		entt::registry m_Registry;
	private:
		template<typename T>
		void OnComponentAdded(Entity entity, T& component);

		friend class Entity;
		//friend class SceneSerializer;
		//friend class SceneHierarchyPanel;
	};	
}
