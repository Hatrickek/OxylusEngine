#pragma once
#include "JoltBuild.h"
#include "PhyiscsInterfaces.h"

namespace Oxylus {
  class Physics {
  public:
    static constexpr uint32_t MAX_BODIES = 1024;
    static constexpr uint32_t MAX_BODY_PAIRS = 1024;
    static constexpr uint32_t MAX_CONTACT_CONSTRAINS = 1024;
    static BPLayerInterfaceImpl s_LayerInterface;
    static ObjectVsBroadPhaseLayerFilterImpl s_ObjectVsBroadPhaseLayerFilterInterface;
    static ObjectLayerPairFilterImpl s_ObjectLayerPairFilterInterface;
    static JPH::TempAllocatorImpl* s_TempAllocator;

    static void InitPhysics();
    static void ShutdownPhysics();
  };
}
