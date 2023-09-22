#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/quaternion.hpp>
#include "Utils/Profiler.h"

namespace Oxylus::Math {
bool DecomposeTransform(const glm::mat4& transform, glm::vec3& translation, glm::vec3& rotation, glm::vec3& scale);

template <typename T>
static T SmoothDamp(const T& current,
                    const T& target,
                    T& currentVelocity,
                    float smoothTime,
                    float maxSpeed,
                    float deltaTime) {
  OX_SCOPED_ZONE;
  // Based on Game Programming Gems 4 Chapter 1.10
  smoothTime = glm::max(0.0001F, smoothTime);
  const float omega = 2.0f / smoothTime;

  const float x = omega * deltaTime;
  const float exp = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);

  T change = current - target;
  const T originalTo = target;

  // Clamp maximum speed
  const float maxChange = maxSpeed * smoothTime;

  const float maxChangeSq = maxChange * maxChange;
  const float sqDist = glm::length2(change);
  if (sqDist > maxChangeSq) {
    const float mag = glm::sqrt(sqDist);
    change = change / mag * maxChange;
  }

  const T newTarget = current - change;
  const T temp = (currentVelocity + omega * change) * deltaTime;

  currentVelocity = (currentVelocity - omega * temp) * exp;

  T output = newTarget + (change + temp) * exp;

  // Prevent overshooting
  const T origMinusCurrent = originalTo - current;
  const T outMinusOrig = output - originalTo;

  if (glm::compAdd(origMinusCurrent * outMinusOrig) > 0.0f) {
    output = originalTo;
    currentVelocity = (output - originalTo) / deltaTime;
  }
  return output;
}

static float Lerp(float a, float b, float t) {
  return a + t * (b - a);
}

static float InverseLerp(float a, float b, float value) {
  OX_SCOPED_ZONE;
  const float den = b - a;
  if (den == 0.0f)
    return 0.0f;
  return (value - a) / den;
}

static float InverseLerpClamped(float a, float b, float value) {
  OX_SCOPED_ZONE;
  const float den = b - a;
  if (den == 0.0f)
    return 0.0f;
  return glm::clamp((value - a) / den, 0.0f, 1.0f);
}
}
