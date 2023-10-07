#include "Camera.h"

#include "Core/Input.h"
#include "Utils/OxMath.h"
#include "Utils/Profiler.h"
#include "Vulkan/VulkanRenderer.h"

namespace Oxylus {
Camera::Camera(glm::vec3 position) {
  m_Position = position;
  UpdateViewMatrix();
}

glm::mat4 Camera::GetProjectionMatrixFlipped() const {
  OX_SCOPED_ZONE;
  auto projection = glm::perspective(glm::radians(Fov), Aspect, NearClip, FarClip);
  projection[1][1] *= -1.0f;
  return projection;
}

glm::mat4 Camera::GetProjectionMatrix() const {
  OX_SCOPED_ZONE;
  return glm::perspective(glm::radians(Fov), Aspect, NearClip, FarClip);
}

void Camera::SetNear(float newNear) {
  SetPerspective(Fov, Aspect, newNear, FarClip);
}

void Camera::SetFar(float newFar) {
  SetPerspective(Fov, Aspect, NearClip, newFar);
}

void Camera::Dolly(float z) {
  Translate({0, 0, z});
}

glm::mat4 Camera::GetViewMatrix() const {
  return m_ViewMatrix;
}

Mat4 Camera::GetWorldMatrix() const {
  return glm::translate(glm::mat4(1.0f), m_Position) *
         glm::toMat4(glm::quat(Vec3(GetPitch(), GetYaw(), m_Tilt)));
}

void Camera::SetPosition(const glm::vec3 pos) {
  m_Position = pos;
}

void Camera::SetPerspective(float fov, float aspect, float znear, float zfar) {
  this->Fov = fov;
  this->NearClip = znear;
  this->FarClip = zfar;
  m_Perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
}

void Camera::SetFov(const float fov) {
  this->Fov = fov;
  m_Perspective = glm::perspective(glm::radians(fov),
    static_cast<float>(VulkanRenderer::get_viewport_width()) / static_cast<float>(VulkanRenderer::get_viewport_height()),
    NearClip,
    FarClip);
}

void Camera::UpdateAspectRatio(const float aspect) {
  m_Perspective = glm::perspective(glm::radians(Fov), aspect, NearClip, FarClip);
}

void Camera::UpdateAspectRatio(const VkExtent2D& size) {
  UpdateAspectRatio(static_cast<float>(size.width) / static_cast<float>(size.height));
}

glm::vec3 Camera::GetFront() const {
  return m_Forward;
}

glm::vec3 Camera::GetRight() const {
  return m_Right;
}

const glm::vec3& Camera::GetPosition() const {
  return m_Position;
}

void Camera::Translate(const glm::vec3& delta) {
  const glm::vec3 camFront = GetFront();
  glm::vec3 camUp = glm::vec3(0.0f, 1.0f, 0.0f);
  const glm::vec3 camLeft = glm::cross(camFront, camUp);
  camUp = glm::cross(camFront, camLeft);
  const glm::vec3 result = camLeft * delta.x + camUp * -delta.y + camFront * delta.z;
  m_Position += result;
}

void Camera::Update() {
  UpdateViewMatrix();
}

void Camera::Update(const glm::vec3& pos, const glm::vec3& rotation) {
  OX_SCOPED_ZONE;
  SetPosition(pos);
  SetPitch(rotation.x);
  SetYaw(rotation.y);
  m_Tilt = rotation.z;
  UpdateViewMatrix();
}

void Camera::UpdateViewMatrix() {
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

  SkyboxView = m_ViewMatrix;
  SkyboxView[3] = glm::vec4(0, 0, 0, 1);

  Aspect = (float)VulkanRenderer::get_viewport_width() / (float)VulkanRenderer::get_viewport_height();
}

Mat4 Camera::GenerateViewMatrix(const Vec3& position, const Vec3& viewDir, const Vec3& up) {
  return glm::lookAt(position, position + viewDir, up);
}
}
