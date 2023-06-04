#pragma once
#include <map>

#include "JoltBuild.h"
#include "PhyiscsInterfaces.h"

namespace Oxylus {
  class Physics {
  public:
    using EntityLayer = uint16_t;

    struct EntityLayerData {
      std::string Name = "Layer";
      EntityLayer Flags = 0xFFFF;
      uint8_t Index = 1;
    };

    static std::map<EntityLayer, EntityLayerData> LayerCollisionMask;

    static constexpr uint32_t MAX_BODIES = 1024;
    static constexpr uint32_t MAX_BODY_PAIRS = 1024;
    static constexpr uint32_t MAX_CONTACT_CONSTRAINS = 1024;
    static BPLayerInterfaceImpl s_LayerInterface;
    static ObjectVsBroadPhaseLayerFilterImpl s_ObjectVsBroadPhaseLayerFilterInterface;
    static ObjectLayerPairFilterImpl s_ObjectLayerPairFilterInterface;

    static void Init();
    static void Step(float physicsTs);
    static void Shutdown();

    static JPH::PhysicsSystem* GetPhysicsSystem();
    static JPH::BodyInterface& GetBodyInterface();

  private:
    static JPH::PhysicsSystem* s_PhysicsSystem;
    static JPH::TempAllocatorImpl* s_TempAllocator;
    static JPH::JobSystemThreadPool* s_JobSystem;
  };
}
