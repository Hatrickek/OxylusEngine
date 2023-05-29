#pragma once
#include "Jolt/Physics/Body/BodyInterface.h"
#include "Jolt/Physics/Collision/Shape/Shape.h"

namespace Oxylus {
  class PhysicsUtils {
  public:
    static const char* ShapeTypeToString(JPH::EShapeSubType type);
    static void DebugDraw(const JPH::BodyInterface* bodyInterface, JPH::BodyID bodyID);
  };
}
