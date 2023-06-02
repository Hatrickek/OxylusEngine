#pragma once
#include "Jolt/Physics/Body/BodyInterface.h"
#include "Jolt/Physics/Collision/Shape/Shape.h"

namespace Oxylus {
  class PhysicsUtils {
  public:
    static const char* ShapeTypeToString(JPH::EShapeSubType type);
    static const char* MotionTypeToString(JPH::EMotionType type);
    static JPH::EMotionType StringToMotionType(const std::string& str);
    static void DebugDraw(const JPH::BodyInterface* bodyInterface, JPH::BodyID bodyID);
  };
}
