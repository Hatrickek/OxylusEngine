#pragma once

#include "Core/Types.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Frustum.h"

#include "Physics/RayCast.hpp"

struct VkExtent2D;

namespace ox {
class Camera {
public:
  Camera(Vec3 position = Vec3(0.0f, 0.0f, 0.0f));

  void update();
  void update(const Vec3& pos, const Vec3& rotation);

  Mat4 get_projection_matrix() const { return matrices.projection_matrix; }
  Mat4 get_inv_projection_matrix() const { return inverse(matrices.projection_matrix); }
  Mat4 get_view_matrix() const { return matrices.view_matrix; }
  Mat4 get_inv_view_matrix() const { return inverse(matrices.view_matrix); }
  Mat4 get_inverse_projection_view() const { return inverse(matrices.projection_matrix * matrices.view_matrix); }

  Mat4 get_previous_projection_matrix() const { return previous_matrices.projection_matrix; }
  Mat4 get_previous_inv_projection_matrix() const { return inverse(previous_matrices.projection_matrix); }
  Mat4 get_previous_view_matrix() const { return previous_matrices.view_matrix; }
  Mat4 get_previous_inv_view_matrix() const { return inverse(previous_matrices.view_matrix); }
  Mat4 get_previous_inverse_projection_view() const { return inverse(previous_matrices.projection_matrix * previous_matrices.view_matrix); }

  const Vec3& get_position() const { return m_position; }
  void set_position(const Vec3 pos) { m_position = pos; }

  float get_yaw() const { return m_yaw; }
  void set_yaw(const float value) { m_yaw = value; }

  float get_pitch() const { return m_pitch; }
  void set_pitch(const float value) { m_pitch = value; }

  float get_tilt() const { return m_tilt; }
  void set_tilt(const float value) { m_tilt = value; }

  float get_near() const { return near_clip; }
  void set_near(float new_near) { near_clip = new_near; }

  float get_far() const { return far_clip; }
  void set_far(float new_far) { far_clip = new_far; }

  float get_fov() const { return m_fov; }
  void set_fov(float fov) { m_fov = fov; }

  float get_aspect() const { return m_aspect; }

  Vec3 get_forward() const { return m_forward; }
  Vec3 get_right() const { return m_right; }

  Vec2 get_jitter() const { return jitter; }
  Vec2 get_previous_jitter() const { return jitter_prev; }
  void set_jitter(const Vec2 value) { jitter = value; }

  void update_view_matrix();
  void update_projection_matrix();
  Frustum get_frustum();
  RayCast get_screen_ray(const Vec2& screen_pos) const;

private:
  float m_fov = 60.0f;
  float m_aspect;
  float far_clip = 1000.0f;
  float near_clip = 0.01f;
  Vec2 jitter = {};
  Vec2 jitter_prev = {};

  uint32_t aspect_ratio_w = 1;
  uint32_t aspect_ratio_h = 1;

  struct Matrices {
    Mat4 view_matrix;
    Mat4 projection_matrix;
  };

  Matrices matrices;
  Matrices previous_matrices;

  Vec3 m_position;
  Vec3 m_forward;
  Vec3 m_right;
  Vec3 m_up;
  float m_yaw = 0;
  float m_pitch = 0;
  float m_tilt = 0;
};
} // namespace ox
