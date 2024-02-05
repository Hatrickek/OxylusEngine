#include "JoltHelpers.h"

#include "JoltBuild.h"

namespace Ox {
Vec3 convert_jolt_vec3(const JPH::Vec3& vec) { return {vec.GetX(), vec.GetY(), vec.GetZ()}; }
Vec4 convert_jolt_vec4(const JPH::Vec4& vec) { return {vec.GetX(), vec.GetY(), vec.GetZ(), vec.GetW()}; }
AABB convert_jolt_aabb(const JPH::AABox& aabb) { return {convert_jolt_vec3(aabb.mMin), convert_jolt_vec3(aabb.mMax)}; }
}
