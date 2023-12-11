#include "BoundingVolume.h"

#include "Frustum.h"

#include "Utils/Profiler.h"

namespace Oxylus {
AABB::AABB(const AABB& other) {
  min = other.min;
  max = other.max;
}

void AABB::translate(const Vec3& translation) {
  OX_SCOPED_ZONE;
  min += translation;
  max += translation;
}

void AABB::scale(const Vec3& scale) {
  OX_SCOPED_ZONE;
  min *= scale;
  max *= scale;
}

void AABB::rotate(const Mat3& rotation) {
  OX_SCOPED_ZONE;
  const auto center = get_center();
  const auto extents = get_extents();

  const auto rotated_extents = Vec3(rotation * Vec4(extents, 1.0f));

  min = center - rotated_extents;
  max = center + rotated_extents;
}

void AABB::transform(const Mat4& transform) {
  OX_SCOPED_ZONE;
  const Vec3 new_center = transform * Vec4(get_center(), 1.0f);
  const Vec3 old_edge = get_size() * 0.5f;
  const Vec3 new_edge = Vec3(
    glm::abs(transform[0][0]) * old_edge.x + glm::abs(transform[1][0]) * old_edge.y + glm::abs(transform[2][0]) * old_edge.z,
    glm::abs(transform[0][1]) * old_edge.x + glm::abs(transform[1][1]) * old_edge.y + glm::abs(transform[2][1]) * old_edge.z,
    glm::abs(transform[0][2]) * old_edge.x + glm::abs(transform[1][2]) * old_edge.y + glm::abs(transform[2][2]) * old_edge.z
  );

  min = new_center - new_edge;
  max = new_center + new_edge;
}

AABB AABB::get_transformed(const Mat4& transform) const {
  AABB aabb(*this);
  aabb.transform(transform);
  return aabb;
}

void AABB::merge(const AABB& other) {
  OX_SCOPED_ZONE;
  if (other.min.x < min.x)
    min.x = other.min.x;
  if (other.min.y < min.y)
    min.y = other.min.y;
  if (other.min.z < min.z)
    min.z = other.min.z;
  if (other.max.x > max.x)
    max.x = other.max.x;
  if (other.max.y > max.y)
    max.y = other.max.y;
  if (other.max.z > max.z)
    max.z = other.max.z;
}

//https://gdbooks.gitbooks.io/3dcollisions/content/Chapter2/static_aabb_plane.html
bool AABB::is_on_or_forward_plane(const Plane& plane) const {
  OX_SCOPED_ZONE;
  // projection interval radius of b onto L(t) = b.c + t * p.n
  const auto extent = get_extents();
  const auto center = get_center();
  const float r = extent.x * std::abs(plane.normal.x) + extent.y * std::abs(plane.normal.y) +
                  extent.z * std::abs(plane.normal.z);

  return -r <= plane.get_distance(center);
}

bool AABB::is_on_frustum(const Frustum& frustum) const {
  OX_SCOPED_ZONE;
  return is_on_or_forward_plane(frustum.left_face) &&
         is_on_or_forward_plane(frustum.right_face) &&
         is_on_or_forward_plane(frustum.top_face) &&
         is_on_or_forward_plane(frustum.bottom_face) &&
         is_on_or_forward_plane(frustum.near_face) &&
         is_on_or_forward_plane(frustum.far_face);
}
}
