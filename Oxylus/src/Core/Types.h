#pragma once

#include <glm/fwd.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext/quaternion_float.hpp>

namespace ox {
using Vec2 = glm::vec2;
using DVec2 = glm::dvec2;
using IVec2 = glm::ivec2;
using UVec2 = glm::uvec2;

using Vec3 = glm::vec3;
using DVec3 = glm::dvec3;
using IVec3 = glm::ivec3;
using UVec3 = glm::uvec3;

using Vec4 = glm::vec4;
using DVec4 = glm::dvec4;
using IVec4 = glm::ivec4;
using UVec4 = glm::uvec4;

using Mat3 = glm::mat3;
using IMat3 = glm::imat3x3;
using UMat3 = glm::umat3x3;

using Mat4 = glm::mat4;
using IMat4 = glm::imat4x4;
using UMat4 = glm::umat4x4;

using Quat = glm::quat;
}
