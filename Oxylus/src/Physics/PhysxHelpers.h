#pragma once
#include "PxPhysicsAPI.h"

#include "glm/glm.hpp"

namespace Oxylus {
  struct TransformComponent;

  glm::vec3 PhysXVec3ToGlmVec3(const physx::PxVec3& vec3);
  glm::vec3 PhysXVec3ToGlmVec3(const physx::PxQuat& quat);
  glm::quat PhysXQuatToglmQuat(const physx::PxQuat& quat);
  physx::PxTransform TransformComponentToPxTransform(const TransformComponent& transformComponent);
  physx::PxVec3 glmVec3ToPhysXVec3(const glm::vec3& vec3);
}
