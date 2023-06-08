#include "src/oxpch.h"
#include "PhyiscsInterfaces.h"

#include "PhysicsMaterial.h"
#include "Scene/Scene.h"
#include "Utils/Profiler.h"


bool ObjectLayerPairFilterImpl::ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const {
  using namespace JPH;
  switch (inObject1) {
    case PhysicsLayers::NON_MOVING: return inObject2 == PhysicsLayers::MOVING; // Non moving only collides with moving
    case PhysicsLayers::MOVING: return true; // Moving collides with everything
    default:
      OX_CORE_ASSERT(false);
      return false;
  }
}

BPLayerInterfaceImpl::BPLayerInterfaceImpl() {
  // Create a mapping table from object to broad phase layer
  mObjectToBroadPhase[PhysicsLayers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
  mObjectToBroadPhase[PhysicsLayers::MOVING] = BroadPhaseLayers::MOVING;
}

JPH::uint BPLayerInterfaceImpl::GetNumBroadPhaseLayers() const {
  return BroadPhaseLayers::NUM_LAYERS;
}

JPH::BroadPhaseLayer BPLayerInterfaceImpl::GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const {
  using namespace JPH;
  OX_CORE_ASSERT(inLayer < PhysicsLayers::NUM_LAYERS);
  return mObjectToBroadPhase[inLayer];
}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
const char* BPLayerInterfaceImpl::GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const {
  using namespace JPH;
  switch ((JPH::BroadPhaseLayer::Type)inLayer) {
    case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING: return "NON_MOVING";
    case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING: return "MOVING";
    default: OX_CORE_ASSERT(false);
      return "INVALID";
  }
}
#endif

bool ObjectVsBroadPhaseLayerFilterImpl::ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const {
  using namespace JPH;
  switch (inLayer1) {
    case PhysicsLayers::NON_MOVING: return inLayer2 == BroadPhaseLayers::MOVING;
    case PhysicsLayers::MOVING: return true;
    default:
      OX_CORE_ASSERT(false);
      return false;
  }
}

void Physics3DBodyActivationListener::OnBodyActivated(const JPH::BodyID& inBodyID, JPH::uint64 inBodyUserData) {
  OX_SCOPED_ZONE;

  /* Body Activated */
}
void Physics3DBodyActivationListener::OnBodyDeactivated(const JPH::BodyID& inBodyID, JPH::uint64 inBodyUserData) {
  OX_SCOPED_ZONE;

  /* Body Deactivated */
}

void Physics3DContactListener::GetFrictionAndRestitution(const JPH::Body& inBody, const JPH::SubShapeID& inSubShapeID, float& outFriction, float& outRestitution) {
  OX_SCOPED_ZONE;

  // Get the material that corresponds to the sub shape ID
  const JPH::PhysicsMaterial* material = inBody.GetShape()->GetMaterial(inSubShapeID);
  if (material == JPH::PhysicsMaterial::sDefault) {
    outFriction = inBody.GetFriction();
    outRestitution = inBody.GetRestitution();
  }
  else {
    const auto* phyMaterial = static_cast<const PhysicsMaterial3D*>(material);
    outFriction = phyMaterial->Friction;
    outRestitution = phyMaterial->Restitution;
  }
}
void Physics3DContactListener::OverrideContactSettings(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) {
  OX_SCOPED_ZONE;

  // Get the custom friction and restitution for both bodies
  float friction1, friction2, restitution1, restitution2;
  GetFrictionAndRestitution(inBody1, inManifold.mSubShapeID1, friction1, restitution1);
  GetFrictionAndRestitution(inBody2, inManifold.mSubShapeID2, friction2, restitution2);

  // Use the default formulas for combining friction and restitution
  ioSettings.mCombinedFriction = JPH::sqrt(friction1 * friction2);
  ioSettings.mCombinedRestitution = JPH::max(restitution1, restitution2);
}
JPH::ValidateResult Physics3DContactListener::OnContactValidate(const JPH::Body &inBody1, const JPH::Body &inBody2, JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult &inCollisionResult) {
  OX_SCOPED_ZONE;

  return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
}

void Physics3DContactListener::OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) {
  OX_SCOPED_ZONE;

  OverrideContactSettings(inBody1, inBody2, inManifold, ioSettings);

  m_Scene->OnContactAdded(inBody1, inBody2, inManifold, ioSettings);
}

void Physics3DContactListener::OnContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) {
  OX_SCOPED_ZONE;

  OverrideContactSettings(inBody1, inBody2, inManifold, ioSettings);
}

void Physics3DContactListener::OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) {
  OX_SCOPED_ZONE;

  /* On Collision Exit */
}
