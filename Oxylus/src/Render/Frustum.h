#pragma once
#include "Core/Types.h"

namespace ox {
struct Plane {
  Vec3 normal = {0.f, 1.f, 0.f}; // unit vector
  float distance = 0.f;          // Distance with origin

  Plane() = default;
  Plane(Vec3 norm) : normal(normalize(norm)) {}

  Plane(const Vec3& p1, const Vec3& norm) : normal(normalize(norm)),
                                            distance(dot(normal, p1)) {}

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

  void init() {
    planes[0] = &top_face;
    planes[1] = &bottom_face;
    planes[2] = &right_face;
    planes[3] = &left_face;
    planes[4] = &far_face;
    planes[5] = &near_face;
  }

  bool is_inside(const Vec3& point) const {
    for (const auto plane : planes) {
      if (plane->get_distance(point) < 0.0f) {
        return false;
      }
    }

    return true;
  }

  static Frustum from_matrix(const Mat4& view_projection) {
    Frustum frustum = {};

    frustum.near_face = Plane(view_projection[2]);

    // Far plane:
    frustum.far_face = Plane(view_projection[3] - view_projection[2]);

    // Left plane:
    frustum.left_face = Plane(view_projection[3] + view_projection[0]);

    // Right plane:
    frustum.right_face = Plane(view_projection[3] - view_projection[0]);

    // Top plane:
    frustum.top_face = Plane(view_projection[3] - view_projection[1]);

    // Bottom plane:
    frustum.bottom_face = Plane(view_projection[3] + view_projection[1]);

    frustum.init();

    return frustum;
  }
};
}
