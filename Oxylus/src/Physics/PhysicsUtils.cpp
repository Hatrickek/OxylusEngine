#include "oxpch.h"
#include "PhysicsUtils.h"

#include <glm/gtc/type_ptr.hpp>

#include "Render/DebugRenderer.h"

namespace Oxylus {
  const char* PhysicsUtils::ShapeTypeToString(JPH::EShapeSubType type) {
    switch (type) {
      case JPH::EShapeSubType::Sphere: return "Sphere";
      case JPH::EShapeSubType::Box: return "Box";
      case JPH::EShapeSubType::Triangle: return "Triangle";
      case JPH::EShapeSubType::Capsule: return "Capsule";
      case JPH::EShapeSubType::TaperedCapsule: return "TaperedCapsule";
      case JPH::EShapeSubType::Cylinder: return "Cylinder";
      case JPH::EShapeSubType::ConvexHull: return "ConvexHull";
      case JPH::EShapeSubType::Mesh: return "Mesh";
      default: return "Empty";
    }
  }

  void PhysicsUtils::DebugDraw(const JPH::BodyInterface* bodyInterface, JPH::BodyID bodyID) {
    switch (bodyInterface->GetShape(bodyID)->GetSubType()) {
      case JPH::EShapeSubType::Sphere: {
        DebugRenderer::DrawSphere(glm::make_vec3(bodyInterface->GetPosition(bodyID).mF32),
          glm::make_vec3(bodyInterface->GetShape(bodyID)->GetLocalBounds().GetSize().mF32),
          Vec4(0, 1, 0, 1),
          glm::make_vec3(bodyInterface->GetRotation(bodyID).GetEulerAngles().mF32));
        break;
      }
      case JPH::EShapeSubType::Box: {
        DebugRenderer::DrawBox(glm::make_vec3(bodyInterface->GetPosition(bodyID).mF32),
          glm::make_vec3(bodyInterface->GetShape(bodyID)->GetLocalBounds().GetSize().mF32),
          Vec4(0, 1, 0, 1),
          glm::make_vec3(bodyInterface->GetRotation(bodyID).GetEulerAngles().mF32));
        break;
      }
      case JPH::EShapeSubType::Capsule: {
        break;
      }
      case JPH::EShapeSubType::TaperedCapsule: {
        break;
      }
      case JPH::EShapeSubType::Cylinder: {
        break;
      }
      case JPH::EShapeSubType::ConvexHull: {
        break;
      }
      case JPH::EShapeSubType::Mesh: {
        break;
      }
      default: {
        DebugRenderer::DrawBox(glm::make_vec3(bodyInterface->GetPosition(bodyID).mF32),
          glm::make_vec3(bodyInterface->GetShape(bodyID)->GetLocalBounds().GetSize().mF32),
          Vec4(0, 1, 0, 1),
          glm::make_vec3(bodyInterface->GetRotation(bodyID).GetEulerAngles().mF32));
      }
    }
  }
}
