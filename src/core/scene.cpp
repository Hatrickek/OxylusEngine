#include "glm/glm.hpp"
#include "scene.h"
#include "draw.h"
#include "entity.h"
#include "resources.h"

namespace FlatEngine {
	Scene::Scene() = default;
	Scene::~Scene() = default;

	Entity Scene::CreateEntity(const std::string& name)
	{
		Entity entity = { m_Registry.create(), this };
		entity.AddComponent<TransformComponent>();
		auto& tag = entity.AddComponent<TagComponent>();
		tag.Tag = name.empty() ? "Entity" : name;
		FE_LOG_INFO("Created an entity: {}", tag.Tag);
		return entity;
	}
	void Scene::DestroyEntity(Entity entity)
	{
		m_Registry.destroy(entity);
	}
	void Scene::OnUpdate(float deltaTime) {
		//Render mesh components
		{
			auto group= m_Registry.group<TransformComponent>(entt::get<MeshRendererComponent>);
			for(auto entity : group) {
				auto [transform, meshrenderer] = group.get<TransformComponent, MeshRendererComponent>(entity);
				Draw::DrawModel(meshrenderer, transform.Translation, transform.Scale, transform.Rotation);
			}
		}
		{
			auto group = m_Registry.group<TransformComponent>(entt::get<PrimitiveRendererComponent>);
			for(auto entity : group ) {
				auto [transform, primitiveRenderer] = group.get<TransformComponent, PrimitiveRendererComponent>(entity);
				Draw::DrawPrimitive(primitiveRenderer, transform.Translation, transform.Scale, transform.Rotation);
			}
		}
	}
	template<typename T>
	void Scene::OnComponentAdded(Entity entity, T& component)
	{
		
	}

	template<>
	void Scene::OnComponentAdded<TransformComponent>(Entity entity, TransformComponent& component)
	{
	}

	template<>
	void Scene::OnComponentAdded<MeshRendererComponent>(Entity entity, MeshRendererComponent& component)
	{
		if(!component.model) {component.model = Resources::GetDefaultCube();}			
		if(!component.shader) {
			component.shader = Resources::GetDefaultShader();
			component.color = WHITE;
		}
	}
	template<>
	void Scene::OnComponentAdded<PrimitiveRendererComponent>(Entity entity, PrimitiveRendererComponent& component)
	{
		component.shader = Resources::GetDefaultShader();
		component.primitive = CUBE;
	}

}
