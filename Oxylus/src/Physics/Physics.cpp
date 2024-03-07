#include "Physics.h"

#include <cstdarg>

#include "JoltHelpers.h"
#include "RayCast.h"

#include "Core/Base.h"

#include "Jolt/RegisterTypes.h"
#include "Jolt/Physics/Collision/CastResult.h"
#include "Jolt/Physics/Collision/RayCast.h"

#include "Utils/Log.h"
#include "Utils/Profiler.h"

namespace ox {
BPLayerInterfaceImpl Physics::layer_interface;
JPH::TempAllocatorImpl* Physics::temp_allocator = nullptr;
ObjectVsBroadPhaseLayerFilterImpl Physics::object_vs_broad_phase_layer_filter_interface;
ObjectLayerPairFilterImpl Physics::object_layer_pair_filter_interface;
JPH::PhysicsSystem* Physics::physics_system = nullptr;
JPH::JobSystemThreadPool* Physics::job_system = nullptr;

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

  temp_allocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024);

  job_system = new JPH::JobSystemThreadPool();
  job_system->Init(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, (int)std::thread::hardware_concurrency() - 1);
  physics_system = new JPH::PhysicsSystem();
  physics_system->Init(
    MAX_BODIES,
    0,
    MAX_BODY_PAIRS,
    MAX_CONTACT_CONSTRAINS,
    layer_interface,
    object_vs_broad_phase_layer_filter_interface,
    object_layer_pair_filter_interface);
}

void Physics::step(float physicsTs) {
  OX_SCOPED_ZONE;

  OX_CORE_ASSERT(physics_system, "Physics system not initialized")

  physics_system->Update(physicsTs, 1, temp_allocator, job_system);
}

void Physics::shutdown() {
  JPH::UnregisterTypes();
  delete JPH::Factory::sInstance;
  JPH::Factory::sInstance = nullptr;
  delete temp_allocator;
  delete physics_system;
  delete job_system;
}

JPH::PhysicsSystem* Physics::get_physics_system() {
  OX_SCOPED_ZONE;

  OX_CORE_ASSERT(physics_system, "Physics system not initialized")

  return physics_system;
}

JPH::BodyInterface& Physics::get_body_interface() {
  return physics_system->GetBodyInterface();
}

const JPH::BroadPhaseQuery& Physics::get_broad_phase() {
  return physics_system->GetBroadPhaseQuery();
}

JPH::AllHitCollisionCollector<JPH::RayCastBodyCollector> Physics::cast_ray(const RayCast& ray_cast) {
  JPH::AllHitCollisionCollector<JPH::RayCastBodyCollector> collector;
  const JPH::RayCast ray{convert_to_jolt_vec3(ray_cast.get_origin()), convert_to_jolt_vec3(ray_cast.get_direction())};
  get_broad_phase().CastRay(ray, collector);

  return collector;
}
}
