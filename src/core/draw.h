#pragma once
#include <glm/glm.hpp>

#include "shader.h"
#include "components.h"
namespace FlatEngine {
	class Draw {
	public:
		static void DrawModel(const MeshRendererComponent& mrc,glm::vec3 position, glm::vec3 scale,glm::vec3 rotation);
		static glm::mat4 DrawCube(Shader& shader,glm::vec4 color,glm::vec3 position, glm::vec3 scale, glm::vec3 rotation = glm::vec3(0));
		static void DrawPrimitive(PrimitiveRendererComponent& prc, glm::vec3 position, glm::vec3 scale, glm::vec3 rotation);
		static void render_quad();
	private:
		static void render_cube();
	};
}
