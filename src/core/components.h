#pragma once

#include <glm/glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <string>
#include <glm/gtx/quaternion.hpp>
#include <utility>

#include "model.h"
#include "utility.h"

namespace FlatEngine {
	#define WHITE glm::vec4(1,1,1,1)
	#define BLANK glm::vec4(0,0,0,0)
	#define RED   glm::vec4(1,0,0,1)
	#define GREEN glm::vec4(0,1,0,1)
	#define BLUE  glm::vec4(0,0,1,1)

	enum Primitive {
		NONE,
		CUBE,
		QUAD
	};

	struct TagComponent
	{
		std::string Tag;

		TagComponent() = default;
		TagComponent(const TagComponent&) = default;
		TagComponent(const std::string& tag)
			: Tag(tag) {}
	};

	struct TransformComponent
	{
		glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Rotation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Scale = { 1.0f, 1.0f, 1.0f };

		TransformComponent() = default;
		TransformComponent(const TransformComponent&) = default;
		TransformComponent(const glm::vec3& translation)
			: Translation(translation) {}

		glm::mat4 GetTransform() const
		{
			glm::mat4 rotation = glm::toMat4(glm::quat(Rotation));

			return glm::translate(glm::mat4(1.0f), Translation)* rotation * glm::scale(glm::mat4(1.0f), Scale);
		}
	};
	struct MeshRendererComponent {
		Ref<Model> model;
		glm::mat4 m_model = glm::mat4(1.0f);
		Ref<Shader> shader;
		glm::vec4 color = BLANK;
		MeshRendererComponent() = default;
		MeshRendererComponent (Ref<Model> model, Ref<Shader> shader, glm::vec4 color)
		: model(std::move(model)), shader(std::move(shader)), color(color){} 
	};
	struct PrimitiveRendererComponent {
		Primitive primitive = CUBE;
		Ref<Shader> shader;
		glm::mat4 m_model = glm::mat4(1.0f);
		glm::vec4 color = WHITE;
		PrimitiveRendererComponent() = default;
		PrimitiveRendererComponent(Primitive primitive, Ref<Shader> shader, glm::vec4 color)
		: primitive(primitive), shader(std::move(shader)), color(color){}
	};
	//struct CameraComponent
	//{
	//	SceneCamera Camera;
	//	bool Primary = true; // TODO: think about moving to Scene
	//	bool FixedAspectRatio = false;

	//	CameraComponent() = default;
	//	CameraComponent(const CameraComponent&) = default;
	//};
}
