#pragma once

#include <glm/glm.hpp>

namespace Oxylus {
#define GLSL_BOOL int32_t //GLSL doesn't have one byte bools
using Vec2 = glm::vec2;
using IVec2 = glm::ivec2;
using UVec2 = glm::uvec2;
using Vec3 = glm::vec3;
using IVec3 = glm::ivec3;
using UVec3 = glm::uvec3;
using Vec4 = glm::vec4;
using IVec4 = glm::ivec4;
using Mat4 = glm::mat4;
using Quat = glm::quat;
}
