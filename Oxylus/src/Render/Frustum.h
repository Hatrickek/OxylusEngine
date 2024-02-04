#pragma once
#include "Core/Types.h"

namespace Ox {
enum class Intersection : uint32_t {
  Outside    = 0,
  Intersects = 1,
  Inside     = 2
};

struct Plane {
  Vec3 normal = {0.f, 1.f, 0.f}; // unit vector
  float distance = 0.f;          // Distance with origin

  Plane() = default;

  Plane(const Vec3& p1, const Vec3& norm) : normal(normalize(norm)),
                                            distance(dot(normal, p1)) { }

  float get_distance(const Vec3& point) const { return dot(normal, point) - distance; }
};

struct Frustum {
  Plane top_face;
  Plane bottom_face;

  Plane right_face;
  Plane left_face;

  Plane far_face;
  Plane near_face;

  Plane* planes[6] = {};

  bool is_inside(const Vec3& point) const {
    for (const auto plane : planes) {
      if (plane->get_distance(point) < 0.0f) {
        return false;
      }
    }

    return true;
  }
};
}
