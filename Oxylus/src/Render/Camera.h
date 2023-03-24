#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.hpp>

namespace Oxylus {
  enum CameraMovement {
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT
  };

  constexpr float FOV = 45.0f;
  constexpr float NEARCLIP = 0.01f;
  constexpr float FARCLIP = 1000.0f;

  /**
   * \brief Camera class only to be used for the Editor. For Runtime `Camera Component` must be used.
   */
  class Camera {
  public:
    glm::mat4 SkyboxView;

    bool ShouldMove = true;

    float Fov = FOV;
    float Aspect;
    float FarClip = FARCLIP;
    float NearClip = NEARCLIP;
    bool ConstrainPitch = true;

    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f));

    glm::mat4 GetProjectionMatrixFlipped() const;
    glm::mat4 GetProjectionMatrix() const;
    glm::mat4 GetViewMatrix() const;
    void SetYaw(float value);
    float GetYaw() const;
    void SetPitch(float value);
    float GetPitch() const;
    void SetNear(float newNear);
    void SetFar(float newFar);
    void Dolly(float z);
    void SetPosition(glm::vec3 pos);
    void SetPerspective(float fov, float aspect, float znear, float zfar);
    void SetFov(float fov);
    void Translate(const glm::vec3& delta);
    void UpdateAspectRatio(float aspect);
    void UpdateAspectRatio(const vk::Extent2D& size);
    glm::vec3 GetFront() const;
    glm::vec3 GetRight() const;
    const glm::vec3& GetPosition() const;
    void Update();
    void Update(const glm::vec3& pos, const glm::vec3& rotation);
    void UpdateViewMatrix();

  private:
    glm::mat4 m_Perspective;
    glm::vec3 m_Position;
    glm::vec3 m_Forward;
    glm::vec3 m_Right;
    glm::vec3 m_Up;
    glm::mat4 m_ViewMatrix;
    float m_Yaw = 0;
    float m_Pitch = 0;
  };
}
