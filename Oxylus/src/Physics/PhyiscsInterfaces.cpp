#include "src/oxpch.h"
#include "PhyiscsInterfaces.h"


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
