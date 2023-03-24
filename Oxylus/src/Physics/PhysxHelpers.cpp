#include "oxpch.h"
#include "Physics/PhysxHelpers.h"

#include <glm/detail/type_quat.hpp>

#include "Core/components.h"

namespace Oxylus {
  glm::vec3 PhysXVec3ToGlmVec3(const physx::PxVec3& vec3) {
    return {vec3.x, vec3.y, vec3.z};
  }

  glm::vec3 PhysXVec3ToGlmVec3(const physx::PxQuat& quat) {
    return {quat.x, quat.y, quat.z};
  }

  physx::PxVec3 glmVec3ToPhysXVec3(const glm::vec3& vec3) {
    return {vec3.x, vec3.y, vec3.z};
  }

  physx::PxTransform TransformComponentToPxTransform(const TransformComponent& transformComponent) {
    physx::PxTransform res;
    res.p.x = transformComponent.Translation.x;
    res.p.y = transformComponent.Translation.y;
    res.p.z = transformComponent.Translation.z;

    res.q.x = transformComponent.Rotation.x;
    res.q.y = transformComponent.Rotation.y;
    res.q.z = transformComponent.Rotation.z;
    return res;
  }

  glm::quat PhysXQuatToglmQuat(const physx::PxQuat& quat) {
    return {quat.w, quat.x, quat.y, quat.z};
  }
}
