#include "Camera.h"

#include "Core/Input.h"
#include "Utils/OxMath.h"
#include "Utils/Profiler.h"

#include "Vulkan/Renderer.h"

namespace Oxylus {
Camera::Camera(glm::vec3 position) {
  m_Position = position;
  update_view_matrix();
}

glm::mat4 Camera::get_projection_matrix_flipped() const {
  OX_SCOPED_ZONE;
  auto projection = glm::perspective(glm::radians(Fov), Aspect, near_clip, far_clip);
  projection[1][1] *= -1.0f;
  return projection;
}

glm::mat4 Camera::get_projection_matrix() const {
  OX_SCOPED_ZONE;
  return glm::perspective(glm::radians(Fov), Aspect, near_clip, far_clip);
}

void Camera::set_near(float newNear) {
  set_perspective(Fov, Aspect, newNear, far_clip);
}

void Camera::set_far(float newFar) {
  set_perspective(Fov, Aspect, near_clip, newFar);
}

void Camera::dolly(float z) {
  translate({0, 0, z});
}

glm::mat4 Camera::get_view_matrix() const {
  return m_ViewMatrix;
}

Mat4 Camera::get_world_matrix() const {
  return glm::translate(glm::mat4(1.0f), m_Position) *
         glm::toMat4(glm::quat(Vec3(get_pitch(), get_yaw(), m_Tilt)));
}

void Camera::set_position(const glm::vec3 pos) {
  m_Position = pos;
}

void Camera::set_perspective(float fov, float aspect, float znear, float zfar) {
  this->Fov = fov;
  this->near_clip = znear;
  this->far_clip = zfar;
  m_Perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
}

void Camera::set_fov(const float fov) {
  this->Fov = fov;
  m_Perspective = glm::perspective(glm::radians(fov),
    static_cast<float>(Renderer::get_viewport_width()) / static_cast<float>(Renderer::get_viewport_height()),
    near_clip,
    far_clip);
}

void Camera::update_aspect_ratio(const float aspect) {
  m_Perspective = glm::perspective(glm::radians(Fov), aspect, near_clip, far_clip);
}

void Camera::update_aspect_ratio(const VkExtent2D& size) {
  update_aspect_ratio(static_cast<float>(size.width) / static_cast<float>(size.height));
}

glm::vec3 Camera::get_front() const {
  return m_Forward;
}

glm::vec3 Camera::get_right() const {
  return m_Right;
}

const glm::vec3& Camera::get_position() const {
  return m_Position;
}

void Camera::translate(const glm::vec3& delta) {
  const glm::vec3 camFront = get_front();
  glm::vec3 camUp = glm::vec3(0.0f, 1.0f, 0.0f);
  const glm::vec3 camLeft = glm::cross(camFront, camUp);
  camUp = glm::cross(camFront, camLeft);
  const glm::vec3 result = camLeft * delta.x + camUp * -delta.y + camFront * delta.z;
  m_Position += result;
}

void Camera::update() {
  update_view_matrix();
}

void Camera::update(const glm::vec3& pos, const glm::vec3& rotation) {
  OX_SCOPED_ZONE;
  set_position(pos);
  set_pitch(rotation.x);
  set_yaw(rotation.y);
  m_Tilt = rotation.z;
  update_view_matrix();
}

void Camera::update_view_matrix() {
  OX_SCOPED_ZONE;
  const float cosYaw = glm::cos(m_Yaw);
  const float sinYaw = glm::sin(m_Yaw);
  const float cosPitch = glm::cos(m_Pitch);
  const float sinPitch = glm::sin(m_Pitch);

  m_Forward.x = cosYaw * cosPitch;
  m_Forward.y = sinPitch;
  m_Forward.z = sinYaw * cosPitch;

  m_Forward = glm::normalize(m_Forward);
  m_Right = glm::normalize(glm::cross(m_Forward, {m_Tilt, 1, m_Tilt}));
  m_Up = glm::normalize(glm::cross(m_Right, m_Forward));

  m_ViewMatrix = glm::lookAt(m_Position, m_Position + m_Forward, m_Up);

  skybox_view = m_ViewMatrix;
  skybox_view[3] = glm::vec4(0, 0, 0, 1);

  Aspect = (float)Renderer::get_viewport_width() / (float)Renderer::get_viewport_height();
}

Mat4 Camera::generate_view_matrix(const Vec3& position, const Vec3& viewDir, const Vec3& up) {
  return glm::lookAt(position, position + viewDir, up);
}
}
