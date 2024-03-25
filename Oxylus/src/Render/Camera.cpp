#include "Camera.hpp"

#include "Utils/OxMath.hpp"
#include "Utils/Profiler.hpp"

#include "Vulkan/Renderer.hpp"

namespace ox {
Camera::Camera(Vec3 position) {
  m_position = position;
  update();
}

void Camera::update() {
  jitter_prev = jitter;
  previous_matrices.projection_matrix = matrices.projection_matrix;
  previous_matrices.view_matrix = matrices.view_matrix;
  update_projection_matrix();
  update_view_matrix();
}

void Camera::update(const Vec3& pos, const Vec3& rotation) {
  OX_SCOPED_ZONE;
  set_position(pos);
  set_pitch(rotation.x);
  set_yaw(rotation.y);
  set_tilt(rotation.z);
  update();
}

void Camera::update_view_matrix() {
  OX_SCOPED_ZONE;
  const float cos_yaw = glm::cos(m_yaw);
  const float sin_yaw = glm::sin(m_yaw);
  const float cos_pitch = glm::cos(m_pitch);
  const float sin_pitch = glm::sin(m_pitch);

  m_forward.x = cos_yaw * cos_pitch;
  m_forward.y = sin_pitch;
  m_forward.z = sin_yaw * cos_pitch;

  m_forward = glm::normalize(m_forward);
  m_right = glm::normalize(glm::cross(m_forward, {m_tilt, 1, m_tilt}));
  m_up = glm::normalize(glm::cross(m_right, m_forward));

  matrices.view_matrix = glm::lookAt(m_position, m_position + m_forward, m_up);

  m_aspect = (float)Renderer::get_viewport_width() / (float)Renderer::get_viewport_height();
}

void Camera::update_projection_matrix() {
  matrices.projection_matrix = glm::perspective(glm::radians(m_fov), m_aspect, far_clip, near_clip); // reversed-z
  matrices.projection_matrix[1][1] *= -1.0f;
}

Frustum Camera::get_frustum() {
  const float half_v_side = get_far() * tanf(glm::radians(m_fov) * .5f);
  const float half_h_side = half_v_side * get_aspect();
  const Vec3 forward_far = get_far() * m_forward;

  Frustum frustum = {
    .top_face = {m_position, cross(m_right, forward_far - m_up * half_v_side)},
    .bottom_face = {m_position, cross(forward_far + m_up * half_v_side, m_right)},
    .right_face = {m_position, cross(forward_far - m_right * half_h_side, m_up)},
    .left_face = {m_position, cross(m_up, forward_far + m_right * half_h_side)},
    .far_face = {m_position + forward_far, -m_forward},
    .near_face = {m_position + get_near() * m_forward, m_forward},
  };

  frustum.init();

  return frustum;
}

RayCast Camera::get_screen_ray(const Vec2& screen_pos) const {
  const Mat4 view_proj_inverse = inverse(get_projection_matrix() * get_view_matrix());

  float screen_x = screen_pos.x / (float)Renderer::get_viewport_width();
  float screen_y = screen_pos.y / (float)Renderer::get_viewport_height();

  screen_x = 2.0f * screen_x - 1.0f;
  screen_y = 2.0f * screen_y - 1.0f;

  Vec4 n = view_proj_inverse * Vec4(screen_x, screen_y, 0.0f, 1.0f);
  n /= n.w;

  Vec4 f = view_proj_inverse * Vec4(screen_x, screen_y, 1.0f, 1.0f);
  f /= f.w;

  return {Vec3(n), normalize(Vec3(f) - Vec3(n))};
}
} // namespace ox
