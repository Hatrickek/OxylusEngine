#pragma once

#include "Core/Types.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Frustum.h"


struct VkExtent2D;

namespace Ox {
class Camera {
public:
  Camera(Vec3 position = Vec3(0.0f, 0.0f, 0.0f));

  void update();
  void update(const Vec3& pos, const Vec3& rotation);

  Mat4 get_projection_matrix() const;
  Mat4 get_projection_matrix_flipped() const;
  Mat4 get_view_matrix() const;
  Mat4 get_world_matrix() const;

  void set_yaw(const float value) { m_yaw = value; }
  void set_pitch(const float value) { m_pitch = value; }

  float get_yaw() const { return m_yaw; }
  float get_pitch() const { return m_pitch; }
  float get_tilt() const { return m_tilt; }

  void set_near(float new_near);
  float get_near() const { return m_near_clip; }
  void set_far(float new_far);
  float get_far() const { return m_far_clip; }

  float get_fov() const { return m_fov; }
  float get_aspect() const { return m_aspect; }

  void dolly(float z);
  void set_position(Vec3 pos);
  void set_perspective(float fov, float aspect, float znear, float zfar);
  void set_fov(float fov);
  void translate(const Vec3& delta);
  void update_aspect_ratio(float aspect);
  void update_aspect_ratio(const VkExtent2D& size);
  Vec3 get_front() const;
  Vec3 get_right() const;
  const Vec3& get_position() const;

  void update_view_matrix();
  static Mat4 generate_view_matrix(const Vec3& position, const Vec3& view_dir, const Vec3& up);

  Frustum get_frustum();

private:
  float m_fov = 60.0f;
  float m_aspect;
  float m_far_clip = 1000.0f;
  float m_near_clip = 0.01f;

  uint32_t aspect_ratio_w = 1;
  uint32_t aspect_ratio_h = 1;

  Mat4 m_perspective;
  Vec3 m_position;
  Vec3 m_forward;
  Vec3 m_right;
  Vec3 m_up;
  Mat4 m_view_matrix;
  float m_yaw = 0;
  float m_pitch = 0;
  float m_tilt = 0;
};
}
