#include "src/oxpch.h"
#include "PhysicsHelpers.h"

#include <glm/gtc/type_ptr.hpp>

#include "PhyiscsInterfaces.h"
#include "Jolt/Physics/Body/BodyCreationSettings.h"
#include "Jolt/Physics/Character/Character.h"
#include "Jolt/Physics/Collision/Shape/BoxShape.h"
#include "Jolt/Physics/Collision/Shape/CapsuleShape.h"
#include "Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h"

namespace Oxylus {
  RigidbodyComponent PhysicsHelpers::CreateBoxRigidbody(JPH::BodyInterface* bodyInterface, const Vec3& position, const Vec3& size) {
    using namespace JPH;
    BodyCreationSettings bcs(
      new BoxShape(Vec3Arg(size.x, size.y, size.z)),
      {position.x, position.y, position.z},
      Quat::sIdentity(),
      EMotionType::Dynamic,
      PhysicsLayers::MOVING);
    bcs.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
    bcs.mMassPropertiesOverride.mMass = 10.0f;
    RigidbodyComponent component;
    component.BodyID = bodyInterface->CreateAndAddBody(bcs, EActivation::Activate);
    component.ShapeSize = glm::make_vec3(bodyInterface->GetShape(component.BodyID)->GetLocalBounds().GetSize().mF32);;
    return component;
  }

  RigidbodyComponent PhysicsHelpers::CreateSphereRigidbody(JPH::BodyInterface* bodyInterface, const Vec3& position, const float radius) {
    using namespace JPH;
    BodyCreationSettings bcs(
      new SphereShape(radius),
      {position.x, position.y, position.z},
      Quat::sIdentity(),
      EMotionType::Dynamic,
      PhysicsLayers::MOVING);
    bcs.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
    bcs.mMassPropertiesOverride.mMass = 10.0f;
    RigidbodyComponent component;
    component.BodyID = bodyInterface->CreateAndAddBody(bcs, EActivation::Activate);
    component.ShapeSize = glm::make_vec3(bodyInterface->GetShape(component.BodyID)->GetLocalBounds().GetSize().mF32);;
    return component;
  }

  void PhysicsHelpers::ScaleShape(RigidbodyComponent& component,
                                  const JPH::BodyInterface* bodyInterface,
                                  bool updateMassProperties,
                                  JPH::EActivation activationMode) {
    const auto shape = bodyInterface->GetShape(component.BodyID)->ScaleShape({component.ShapeSize.x, component.ShapeSize.y, component.ShapeSize.z});
    bodyInterface->SetShape(component.BodyID,
      shape.Get(),
      updateMassProperties,
      activationMode);
    component.ShapeSize = glm::make_vec3(bodyInterface->GetShape(component.BodyID)->GetLocalBounds().GetSize().mF32);
  }

  CharacterControllerComponent PhysicsHelpers::CreateCharachterController(JPH::PhysicsSystem* physicsSystem, const Vec3& pos) {
    CharacterControllerComponent component = {};
    component.Shape = JPH::RotatedTranslatedShapeSettings(
      JPH::Vec3(pos.x, 0.5f * component.CharacterHeightStanding + component.CharacterRadiusStanding, pos.z),
      JPH::Quat::sIdentity(),
      new JPH::CapsuleShape(0.5f * component.CharacterHeightStanding, component.CharacterRadiusStanding)).Create().Get();

    // Create character
    Ref<JPH::CharacterSettings> settings = CreateRef<JPH::CharacterSettings>();
    settings->mMaxSlopeAngle = JPH::DegreesToRadians(45.0f);
    settings->mLayer = PhysicsLayers::MOVING;
    settings->mShape = component.Shape;
    settings->mFriction = 0.5f;
    settings->mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -component.CharacterRadiusStanding); // Accept contacts that touch the lower sphere of the capsule
    component.Character = new JPH::Character(settings.get(), JPH::RVec3::sZero(), JPH::Quat::sIdentity(), 0, physicsSystem);
    component.Character->AddToPhysicsSystem(JPH::EActivation::Activate);

    return component;
  }
}
