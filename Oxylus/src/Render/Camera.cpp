#include "Camera.h"

#include "Core/Input.h"
#include "Utils/OxMath.h"
#include "Utils/Profiler.h"

#include "Vulkan/Renderer.h"

namespace Ox {
Camera::Camera(Vec3 position) {
  m_position = position;
  update_view_matrix();
}

Mat4 Camera::get_projection_matrix() const {
  OX_SCOPED_ZONE;
  auto projection = glm::perspective(glm::radians(m_fov), m_aspect, m_near_clip, m_far_clip);
  projection[1][1] *= -1.0f;
  return projection;
}

Mat4 Camera::get_projection_matrix_flipped() const {
  return glm::perspective(glm::radians(m_fov), m_aspect, m_near_clip, m_far_clip);
}

void Camera::set_near(float new_near) {
  set_perspective(m_fov, m_aspect, new_near, m_far_clip);
}

void Camera::set_far(float new_far) {
  set_perspective(m_fov, m_aspect, m_near_clip, new_far);
}

void Camera::dolly(float z) {
  translate({0, 0, z});
}

Mat4 Camera::get_view_matrix() const {
  return m_view_matrix;
}

Mat4 Camera::get_world_matrix() const {
  return glm::translate(Mat4(1.0f), m_position) *
         glm::toMat4(glm::quat(Vec3(get_pitch(), get_yaw(), m_tilt)));
}

void Camera::set_position(const Vec3 pos) {
  m_position = pos;
}

void Camera::set_perspective(float fov, float aspect, float znear, float zfar) {
  this->m_fov = fov;
  this->m_near_clip = znear;
  this->m_far_clip = zfar;
  m_perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
}

void Camera::set_fov(const float fov) {
  this->m_fov = fov;
  m_perspective = glm::perspective(glm::radians(fov),
                                   static_cast<float>(Renderer::get_viewport_width()) / static_cast<float>(Renderer::get_viewport_height()),
                                   m_near_clip,
                                   m_far_clip);
}

void Camera::update_aspect_ratio(const float aspect) {
  m_perspective = glm::perspective(glm::radians(m_fov), aspect, m_near_clip, m_far_clip);
}

void Camera::update_aspect_ratio(const VkExtent2D& size) {
  update_aspect_ratio(static_cast<float>(size.width) / static_cast<float>(size.height));
}

Vec3 Camera::get_front() const {
  return m_forward;
}

Vec3 Camera::get_right() const {
  return m_right;
}

const Vec3& Camera::get_position() const {
  return m_position;
}

void Camera::translate(const Vec3& delta) {
  const Vec3 cam_front = get_front();
  Vec3 cam_up = Vec3(0.0f, 1.0f, 0.0f);
  const Vec3 cam_left = glm::cross(cam_front, cam_up);
  cam_up = glm::cross(cam_front, cam_left);
  const Vec3 result = cam_left * delta.x + cam_up * -delta.y + cam_front * delta.z;
  m_position += result;
}

void Camera::update() {
  update_view_matrix();
}

void Camera::update(const Vec3& pos, const Vec3& rotation) {
  OX_SCOPED_ZONE;
  set_position(pos);
  set_pitch(rotation.x);
  set_yaw(rotation.y);
  set_tilt(rotation.z);
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

Mat4 Camera::generate_view_matrix(const Vec3& position, const Vec3& view_dir, const Vec3& up) {
  return glm::lookAt(position, position + view_dir, up);
}

Frustum Camera::get_frustum() {
  const float half_v_side = get_far() * tanf(glm::radians(m_fov) * .5f);
  const float half_h_side = half_v_side * get_aspect();
  const Vec3 forward_far = get_far() * m_forward;

#if 1
  Frustum frustum = {
    .top_face = {m_position, cross(m_right, forward_far - m_up * half_v_side)},
    .bottom_face = {m_position, cross(forward_far + m_up * half_v_side, m_right)},
    .right_face = {m_position, cross(forward_far - m_right * half_h_side, m_up)},
    .left_face = {m_position, cross(m_up, forward_far + m_right * half_h_side)},
    .far_face = {m_position + forward_far, -m_forward},
    .near_face = {m_position + get_near() * m_forward, m_forward},
  };
#endif

#if 0
  Frustum frustum = {
    .top_face = {m_forward * m_near_clip + m_position, m_forward},
    .bottom_face = {m_position + forward_far, m_forward * -1.0f},
    .right_face = {m_position, cross(m_up, forward_far + m_right * half_h_side)},
    .left_face = {m_position, cross(forward_far - m_right * half_h_side, m_up)},
    .far_face = {m_position, cross(m_right, forward_far - m_up * half_v_side)},
    .near_face = {m_position, cross(forward_far + m_up * half_v_side, m_right)}
  };
#endif

  frustum.planes[0] = &frustum.top_face;
  frustum.planes[1] = &frustum.bottom_face;
  frustum.planes[2] = &frustum.right_face;
  frustum.planes[3] = &frustum.left_face;
  frustum.planes[4] = &frustum.far_face;
  frustum.planes[5] = &frustum.near_face;

  return frustum;
}

RayCast Camera::get_screen_ray(const Vec2& screen_pos) const {
  const Mat4 viewProjInverse = inverse(get_projection_matrix() * get_view_matrix());

  float screenX = screen_pos.x / (float)Renderer::get_viewport_width();
  float screenY = screen_pos.y / (float)Renderer::get_viewport_height();

  screenX = 2.0f * screenX - 1.0f;
  screenY = 2.0f * screenY - 1.0f;

  Vec4 n = viewProjInverse * Vec4(screenX, screenY, 0.0f, 1.0f);
  n /= n.w;

  Vec4 f = viewProjInverse * Vec4(screenX, screenY, 1.0f, 1.0f);
  f /= f.w;

  return {Vec3(n), normalize(Vec3(f) - Vec3(n))};
}
}
