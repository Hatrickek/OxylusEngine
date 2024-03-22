#include "Camera.h"

#include "Utils/OxMath.h"
#include "Utils/Profiler.h"

#include "Vulkan/Renderer.h"

namespace ox {
Camera::Camera(Vec3 position) {
  m_position = position;
  update_view_matrix();
  update_projection_matrix();
}

Mat4 Camera::get_projection_matrix() const {
  OX_SCOPED_ZONE;
  return m_projection_matrix;
}

Mat4 Camera::get_inv_projection_matrix() const { return inverse(m_projection_matrix); }

Mat4 Camera::get_projection_matrix_flipped() const { return glm::perspective(glm::radians(m_fov), m_aspect, far_clip, near_clip); }

void Camera::set_near(float new_near) { near_clip = new_near; }

void Camera::set_far(float new_far) { far_clip = new_far; }

Mat4 Camera::get_view_matrix() const { return m_view_matrix; }

Mat4 Camera::get_inv_view_matrix() const { return inverse(m_view_matrix); }

Mat4 Camera::get_world_matrix() const {
  return glm::translate(Mat4(1.0f), m_position) * glm::toMat4(glm::quat(Vec3(get_pitch(), get_yaw(), m_tilt)));
}

Mat4 Camera::get_inverse_projection_view() const { return inverse(get_projection_matrix() * get_view_matrix()); }

void Camera::set_position(const Vec3 pos) { m_position = pos; }

void Camera::set_fov(const float fov) { this->m_fov = fov; }

Vec3 Camera::get_forward() const { return m_forward; }

Vec3 Camera::get_right() const { return m_right; }

const Vec3& Camera::get_position() const { return m_position; }

void Camera::update() {
  update_projection_matrix();
  update_view_matrix();
}

void Camera::update(const Vec3& pos, const Vec3& rotation) {
  OX_SCOPED_ZONE;
  set_position(pos);
  set_pitch(rotation.x);
  set_yaw(rotation.y);
  set_tilt(rotation.z);
  update_projection_matrix();
  update_view_matrix();
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

  m_view_matrix = glm::lookAt(m_position, m_position + m_forward, m_up);

  m_aspect = (float)Renderer::get_viewport_width() / (float)Renderer::get_viewport_height();
}

void Camera::update_projection_matrix() {
  m_projection_matrix = glm::perspective(glm::radians(m_fov), m_aspect, far_clip, near_clip); // reversed-z
  m_projection_matrix[1][1] *= -1.0f;
}

Mat4 Camera::generate_view_matrix(const Vec3& position, const Vec3& view_dir, const Vec3& up) {
  return glm::lookAt(position, position + view_dir, up);
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
