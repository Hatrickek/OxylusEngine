#pragma once
#include "Core/Types.h"

namespace Oxylus {
struct Plane;
struct Frustum;

struct AABB {
  Vec3 min = {};
  Vec3 max = {};

  AABB() = default;
  AABB(const AABB& other);
    
  AABB(const Vec3 min, const Vec3 max) :
    min(min),
    max(max) {}

  Vec3 get_center() const { return (max + min) * 0.5f; }
  Vec3 get_extents() const { return max - min; }
  Vec3 get_size() const { return get_extents(); }

  void translate(const Vec3& translation);
  void scale(const Vec3& scale);
  void rotate(const Mat3& rotation);
  void transform(const Mat4& transform);
  AABB get_transformed(const Mat4& transform) const;

  void merge(const AABB& other);

  bool is_on_or_forward_plane(const Plane& plane) const;
  bool is_on_frustum(const Frustum& frustum) const;
};
}
