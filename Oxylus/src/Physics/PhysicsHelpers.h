#pragma once

#include "Core/Types.h"
#include "Jolt/Physics/Body/BodyInterface.h"
#include "Jolt/Physics/Collision/Shape/Shape.h"
#include "Core/Components.h"

namespace Oxylus {
  class PhysicsHelpers {
  public:
    static RigidbodyComponent CreateBoxRigidbody(JPH::BodyInterface* bodyInterface, const Vec3& position = {}, const Vec3& size = Vec3(1));
    static RigidbodyComponent CreateSphereRigidbody(JPH::BodyInterface* bodyInterface, const Vec3& position = {}, float radius = 1.0f);

    /// Scale the shape to it's size specified in the component.
    static void ScaleShape(RigidbodyComponent& component,
                           const JPH::BodyInterface* bodyInterface,
                           bool updateMassProperties = true,
                           JPH::EActivation activationMode = JPH::EActivation::Activate);

    static CharacterControllerComponent CreateCharachterController(JPH::PhysicsSystem* physicsSystem,const Vec3& pos = {});
  };
}
