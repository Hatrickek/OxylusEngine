#pragma once
#include "Render/BoundingVolume.h"

namespace JPH {
class AABox;
class Vec3;
class Vec4;
}

namespace ox {
Vec3 convert_from_jolt_vec3(const JPH::Vec3& vec);
JPH::Vec3 convert_to_jolt_vec3(const Vec3& vec);
Vec4 convert_from_jolt_vec4(const JPH::Vec4& vec);
JPH::Vec4 convert_to_jolt_vec4(const Vec4& vec);
AABB convert_jolt_aabb(const JPH::AABox& aabb);
}
