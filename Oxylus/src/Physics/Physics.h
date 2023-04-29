#pragma once
#include "JoltBuild.h"
#include "PhyiscsInterfaces.h"

namespace Oxylus {
  class Physics {
  public:
    static constexpr uint32_t MAX_BODIES = 1024;
    static constexpr uint32_t MAX_BODY_PAIRS = 1024;
    static constexpr uint32_t MAX_CONTACT_CONSTRAINS = 1024;

    static void InitPhysics();
    static void Update(float deltaTime);
    static void Shutdown();

  private:
    static JPH::TempAllocatorImpl* s_TempAllocator;
    static Ref<JPH::PhysicsSystem> s_PhysicsSystem;
    static Ref<JPH::JobSystemThreadPool> s_JobSystem;
    static BPLayerInterfaceImpl s_LayerInterface;
    static ObjectVsBroadPhaseLayerFilterImpl s_ObjectVsBroadPhaseLayerFilterInterface;
    static ObjectLayerPairFilterImpl s_ObjectLayerPairFilterInterface;
  };
}
