#pragma once
#include "Render/BoundingVolume.h"

namespace JPH {
class AABox;
class Vec3;
class Vec4;
}

namespace Ox {
Vec3 convert_jolt_vec3(const JPH::Vec3& vec);
Vec4 convert_jolt_vec4(const JPH::Vec4& vec);
AABB convert_jolt_aabb(const JPH::AABox& aabb);
}
