#pragma once

#include "Core/Types.h"

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
    Mat4 SkyboxView;

    bool ShouldMove = true;

    float Fov = FOV;
    float Aspect;
    float FarClip = FARCLIP;
    float NearClip = NEARCLIP;
    bool ConstrainPitch = true;

    Camera(Vec3 position = Vec3(0.0f, 0.0f, 0.0f));

    Mat4 GetProjectionMatrixFlipped() const;
    Mat4 GetProjectionMatrix() const;
    Mat4 GetViewMatrix() const;
    void SetYaw(float value);
    float GetYaw() const;
    void SetPitch(float value);
    float GetPitch() const;
    void SetNear(float newNear);
    void SetFar(float newFar);
    void Dolly(float z);
    void SetPosition(Vec3 pos);
    void SetPerspective(float fov, float aspect, float znear, float zfar);
    void SetFov(float fov);
    void Translate(const Vec3& delta);
    void UpdateAspectRatio(float aspect);
    void UpdateAspectRatio(const vk::Extent2D& size);
    Vec3 GetFront() const;
    Vec3 GetRight() const;
    const Vec3& GetPosition() const;
    void Update();
    void Update(const Vec3& pos, const Vec3& rotation);
    void UpdateViewMatrix();
    static Mat4 GenerateViewMatrix(const Vec3& position, const Vec3& viewDir, const Vec3& up);

  private:
    Mat4 m_Perspective;
    Vec3 m_Position;
    Vec3 m_Forward;
    Vec3 m_Right;
    Vec3 m_Up;
    Mat4 m_ViewMatrix;
    float m_Yaw = 0;
    float m_Pitch = 0;
  };
}
