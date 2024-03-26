﻿#include "JoltHelpers.hpp"

#include "Jolt/Jolt.h"
#include "Jolt/Geometry/AABox.h"

namespace ox {
Vec3 convert_from_jolt_vec3(const JPH::Vec3& vec) { return {vec.GetX(), vec.GetY(), vec.GetZ()}; }
JPH::Vec3 convert_to_jolt_vec3(const Vec3& vec) { return {vec.x, vec.y, vec.z}; }
Vec4 convert_from_jolt_vec4(const JPH::Vec4& vec) { return {vec.GetX(), vec.GetY(), vec.GetZ(), vec.GetW()}; }
JPH::Vec4 convert_to_jolt_vec4(const Vec4& vec) { return {vec.x, vec.y, vec.z, vec.w}; }
AABB convert_jolt_aabb(const JPH::AABox& aabb) { return {convert_from_jolt_vec3(aabb.mMin), convert_from_jolt_vec3(aabb.mMax)}; }
}
