#pragma once

#include "Core/Types.h"
#include "Jolt/Physics/Body/BodyInterface.h"

namespace Oxylus {
  class PhysicsRigidbodies {
  public:
    static JPH::BodyID CreateBoxBody(JPH::BodyInterface* bodyInterface, const Vec3& position = {}, const Vec3& size = Vec3(1));
    static JPH::BodyID CreateSphereBody(JPH::BodyInterface* bodyInterface, const Vec3& position = {}, float radius = 1.0f);
  };
}
