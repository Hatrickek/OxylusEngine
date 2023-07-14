#include "Physics.h"

#include "Core/Base.h"
#include "Utils/Log.h"
#include "Utils/Profiler.h"

namespace Oxylus {
  BPLayerInterfaceImpl Physics::s_LayerInterface;
  JPH::TempAllocatorImpl* Physics::s_TempAllocator = nullptr;
  ObjectVsBroadPhaseLayerFilterImpl Physics::s_ObjectVsBroadPhaseLayerFilterInterface;
  ObjectLayerPairFilterImpl Physics::s_ObjectLayerPairFilterInterface;
  JPH::PhysicsSystem* Physics::s_PhysicsSystem = nullptr;
  JPH::JobSystemThreadPool* Physics::s_JobSystem = nullptr;

  std::map<Physics::EntityLayer, Physics::EntityLayerData> Physics::LayerCollisionMask =
  {
    {BIT(0), {"Static", static_cast<uint16_t>(0xFFFF), 0}},
    {BIT(1), {"Default", static_cast<uint16_t>(0xFFFF), 1}},
    {BIT(2), {"Player", static_cast<uint16_t>(0xFFFF), 2}},
    {BIT(3), {"Sensor", static_cast<uint16_t>(0xFFFF), 3}},
  };

  static void TraceImpl(const char* inFMT, ...) {
    va_list list;
    va_start(list, inFMT);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), inFMT, list);
    va_end(list);

    OX_CORE_TRACE(buffer);
  }

#ifdef JPH_ENABLE_ASSERTS
  static bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, JPH::uint inLine) {
    OX_CORE_FATAL("{0}:{1}:{2} {3}", inFile, inLine, inExpression, inMessage != nullptr ? inMessage : "");
    return true;
  };
#endif

  void Physics::Init() {
    // TODO: Override default allocators with Oxylus allocators.
    JPH::RegisterDefaultAllocator();

    // Install callbacks
    JPH::Trace = TraceImpl;
#ifdef JPH_ENABLE_ASSERTS
    JPH::AssertFailed = AssertFailedImpl;
#endif

    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    s_TempAllocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024);

    s_JobSystem = new JPH::JobSystemThreadPool();
    s_JobSystem->Init(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, (int)std::thread::hardware_concurrency() - 1);
    s_PhysicsSystem = new JPH::PhysicsSystem();
    s_PhysicsSystem->Init(
      MAX_BODIES,
      0,
      MAX_BODY_PAIRS,
      MAX_CONTACT_CONSTRAINS,
      s_LayerInterface,
      s_ObjectVsBroadPhaseLayerFilterInterface,
      s_ObjectLayerPairFilterInterface);
  }

  void Physics::Step(float physicsTs) {
    OX_SCOPED_ZONE;

    OX_CORE_ASSERT(s_PhysicsSystem, "Physics system not initialized")

    s_PhysicsSystem->Update(physicsTs, 1, s_TempAllocator, s_JobSystem);
  }

  void Physics::Shutdown() {
    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
    delete s_TempAllocator;
    delete s_PhysicsSystem;
    delete s_JobSystem;
  }

  JPH::PhysicsSystem* Physics::GetPhysicsSystem() {
    OX_SCOPED_ZONE;

    OX_CORE_ASSERT(s_PhysicsSystem, "Physics system not initialized")

    return s_PhysicsSystem;
  }

  JPH::BodyInterface& Physics::GetBodyInterface() {
    return GetPhysicsSystem()->GetBodyInterface();
  }
}
