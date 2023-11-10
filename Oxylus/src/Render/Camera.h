#pragma once

#include "Core/Types.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct VkExtent2D;

namespace Oxylus {
constexpr float FOV = 60.0f;
constexpr float NEARCLIP = 0.1f;
constexpr float FARCLIP = 1000.0f;

/**
 * \brief Camera class only to be used for the Editor. For Runtime `Camera Component` must be used.
 */
class Camera {
public:
  Mat4 skybox_view;

  float Fov = FOV;
  float Aspect;
  uint32_t AspectRatioW = 1;
  uint32_t AspectRatioH = 1;
  float far_clip = FARCLIP;
  float near_clip = NEARCLIP;
  bool constrain_pitch = true;

  Camera(Vec3 position = Vec3(0.0f, 0.0f, 0.0f));

  void update();
  void update(const Vec3& pos, const Vec3& rotation);

  Mat4 get_projection_matrix_flipped() const;
  Mat4 get_projection_matrix() const;
  Mat4 get_view_matrix() const;
  Mat4 get_world_matrix() const;

  void set_yaw(const float value) { m_Yaw = value; }
  void set_pitch(const float value) { m_Pitch = value; }

  float get_yaw() const { return m_Yaw; }
  float get_pitch() const { return m_Pitch; }
  float get_tilt() const { return m_Tilt; }

  void set_near(float newNear);
  float get_near() const { return near_clip; }
  void set_far(float newFar);
  float get_far() const { return far_clip; }

  float get_fov() const { return FOV; }
  float get_aspect() const { return Aspect; }

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
  static Mat4 generate_view_matrix(const Vec3& position, const Vec3& viewDir, const Vec3& up);

private:
  Mat4 m_Perspective;
  Vec3 m_Position;
  Vec3 m_Forward;
  Vec3 m_Right;
  Vec3 m_Up;
  Mat4 m_ViewMatrix;
  float m_Yaw = 0;
  float m_Pitch = 0;
  float m_Tilt = 0;
};
}
