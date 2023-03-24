#include "src/oxpch.h"
#include "Camera.h"

#include "Core/Input.h"
#include "Render/Window.h"
#include "Utils/OxMath.h"
#include "Utils/Profiler.h"

namespace Oxylus {
  Camera::Camera(glm::vec3 position) {
    m_Position = position;
    UpdateViewMatrix();
  }

  glm::mat4 Camera::GetProjectionMatrixFlipped() const {
    ZoneScoped;
    auto projection = glm::perspective(glm::radians(Fov), Aspect, NearClip, FarClip);
    projection[1][1] *= -1.0f;
    return projection;
  }

  glm::mat4 Camera::GetProjectionMatrix() const {
    ZoneScoped;
    return glm::perspective(glm::radians(Fov), Aspect, NearClip, FarClip);
  }

  void Camera::SetYaw(const float value) {
    m_Yaw = value;
  }

  float Camera::GetYaw() const {
    return m_Yaw;
  }

  void Camera::SetPitch(const float value) {
    m_Pitch = value;
  }

  float Camera::GetPitch() const {
    return m_Pitch;
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
      static_cast<float>(Window::GetWidth()) / static_cast<float>(Window::GetHeight()),
      NearClip,
      FarClip);
  }

  void Camera::UpdateAspectRatio(const float aspect) {
    m_Perspective = glm::perspective(glm::radians(Fov), aspect, NearClip, FarClip);
  }

  void Camera::UpdateAspectRatio(const vk::Extent2D& size) {
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
    SetPosition(pos);
    SetPitch(rotation.x);
    SetYaw(rotation.y);
    UpdateViewMatrix();
  }

  void Camera::UpdateViewMatrix() {
    ZoneScoped;
    const float cosYaw = glm::cos(m_Yaw);
    const float sinYaw = glm::sin(m_Yaw);
    const float cosPitch = glm::cos(m_Pitch);
    const float sinPitch = glm::sin(m_Pitch);

    m_Forward.x = cosYaw * cosPitch;
    m_Forward.y = sinPitch;
    m_Forward.z = sinYaw * cosPitch;

    m_Forward = glm::normalize(m_Forward);
    m_Right = glm::normalize(glm::cross(m_Forward, {0, 1, 0}));
    m_Up = glm::normalize(glm::cross(m_Right, m_Forward));

    m_ViewMatrix = glm::lookAt(m_Position, m_Position + m_Forward, m_Up);

    SkyboxView = m_ViewMatrix;
    SkyboxView[3] = glm::vec4(0, 0, 0, 1);

    Aspect = (float)Window::GetWidth() / (float)Window::GetHeight();
  }
}
