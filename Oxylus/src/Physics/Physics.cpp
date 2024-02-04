#include "Physics.h"

#include <cstdarg>

#include "Core/Base.h"
#include "Utils/Log.h"
#include "Utils/Profiler.h"

namespace Ox {
BPLayerInterfaceImpl Physics::s_layer_interface;
JPH::TempAllocatorImpl* Physics::s_TempAllocator = nullptr;
ObjectVsBroadPhaseLayerFilterImpl Physics::s_object_vs_broad_phase_layer_filter_interface;
ObjectLayerPairFilterImpl Physics::s_object_layer_pair_filter_interface;
JPH::PhysicsSystem* Physics::s_PhysicsSystem = nullptr;
JPH::JobSystemThreadPool* Physics::s_JobSystem = nullptr;

std::map<Physics::EntityLayer, Physics::EntityLayerData> Physics::layer_collision_mask =
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

  OX_CORE_TRACE("{}", buffer);
}

#ifdef JPH_ENABLE_ASSERTS
  static bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, JPH::uint inLine) {
    OX_CORE_ERROR("{0}:{1}:{2} {3}", inFile, inLine, inExpression, inMessage != nullptr ? inMessage : "");
    return true;
  };
#endif

void Physics::init() {
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
    s_layer_interface,
    s_object_vs_broad_phase_layer_filter_interface,
    s_object_layer_pair_filter_interface);
}

void Physics::step(float physicsTs) {
  OX_SCOPED_ZONE;

  OX_CORE_ASSERT(s_PhysicsSystem, "Physics system not initialized")

  s_PhysicsSystem->Update(physicsTs, 1, s_TempAllocator, s_JobSystem);
}

void Physics::shutdown() {
  JPH::UnregisterTypes();
  delete JPH::Factory::sInstance;
  JPH::Factory::sInstance = nullptr;
  delete s_TempAllocator;
  delete s_PhysicsSystem;
  delete s_JobSystem;
}

JPH::PhysicsSystem* Physics::get_physics_system() {
  OX_SCOPED_ZONE;

  OX_CORE_ASSERT(s_PhysicsSystem, "Physics system not initialized")

  return s_PhysicsSystem;
}

JPH::BodyInterface& Physics::get_body_interface() {
  return get_physics_system()->GetBodyInterface();
}
}
