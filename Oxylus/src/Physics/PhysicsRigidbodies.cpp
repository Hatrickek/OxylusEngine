#include "src/oxpch.h"
#include "PhysicsRigidbodies.h"

#include "PhyiscsInterfaces.h"
#include "Jolt/Physics/Body/BodyCreationSettings.h"
#include "Jolt/Physics/Character/Character.h"
#include "Jolt/Physics/Collision/Shape/BoxShape.h"
#include "Jolt/Physics/Collision/Shape/CapsuleShape.h"
#include "Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h"

namespace Oxylus {
  JPH::BodyID PhysicsRigidbodies::CreateBoxBody(JPH::BodyInterface* bodyInterface, const Vec3& position, const Vec3& size) {
    using namespace JPH;
    BodyCreationSettings bcs(
      new BoxShape(Vec3Arg(size.x, size.y, size.z)),
      {position.x, position.y, position.z},
      Quat::sIdentity(),
      EMotionType::Dynamic,
      PhysicsLayers::MOVING);
    bcs.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
    bcs.mMassPropertiesOverride.mMass = 10.0f;
    return bodyInterface->CreateAndAddBody(bcs, EActivation::Activate);
  }

  JPH::BodyID PhysicsRigidbodies::CreateSphereBody(JPH::BodyInterface* bodyInterface, const Vec3& position, const float radius) {
    using namespace JPH;
    BodyCreationSettings bcs(
      new SphereShape(radius),
      {position.x, position.y, position.z},
      Quat::sIdentity(),
      EMotionType::Dynamic,
      PhysicsLayers::MOVING);
    bcs.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
    bcs.mMassPropertiesOverride.mMass = 10.0f;
    return bodyInterface->CreateAndAddBody(bcs, EActivation::Activate);
  }
}
