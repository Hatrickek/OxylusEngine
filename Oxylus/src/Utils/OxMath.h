#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/quaternion.hpp>

#include "Core/Types.h"

#include "Utils/Profiler.h"

namespace ox::math {
bool decompose_transform(const glm::mat4& transform, Vec3& translation, Vec3& rotation, Vec3& scale);

template <typename T>
static T smooth_damp(const T& current,
                     const T& target,
                     T& current_velocity,
                     float smooth_time,
                     const float max_speed,
                     float delta_time) {
  OX_SCOPED_ZONE;
  // Based on Game Programming Gems 4 Chapter 1.10
  smooth_time = glm::max(0.0001F, smooth_time);
  const float omega = 2.0f / smooth_time;

  const float x = omega * delta_time;
  const float exp = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);

  T change = current - target;
  const T original_to = target;

  // Clamp maximum speed
  const float max_change = max_speed * smooth_time;

  const float max_change_sq = max_change * max_change;
  const float sq_dist = glm::length2(change);
  if (sq_dist > max_change_sq) {
    const float mag = glm::sqrt(sq_dist);
    change = change / mag * max_change;
  }

  const T new_target = current - change;
  const T temp = (current_velocity + omega * change) * delta_time;

  current_velocity = (current_velocity - omega * temp) * exp;

  T output = new_target + (change + temp) * exp;

  // Prevent overshooting
  const T orig_minus_current = original_to - current;
  const T out_minus_orig = output - original_to;

  if (glm::compAdd(orig_minus_current * out_minus_orig) > 0.0f) {
    output = original_to;
    current_velocity = (output - original_to) / delta_time;
  }
  return output;
}

float lerp(float a, float b, float t);
float inverse_lerp(float a, float b, float value);
float inverse_lerp_clamped(float a, float b, float value);
Vec2 world_to_screen(const Vec3& world_pos, const glm::mat4& mvp, float width, float height, float win_pos_x, float win_pos_y);

Vec4 transform(const Vec4& vec, const Mat4& view);
Vec4 transform_normal(const Vec4& vec, const Mat4& mat);
Vec4 transform_coord(const Vec4& vec, const Mat4& view);
}
