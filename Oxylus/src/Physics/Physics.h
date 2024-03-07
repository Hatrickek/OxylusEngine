#pragma once
#include <map>

#include "PhysicsInterfaces.h"

#include "Jolt/Core/JobSystemThreadPool.h"
#include "Jolt/Physics/PhysicsSystem.h"
#include "Jolt/Physics/Collision/CollisionCollectorImpl.h"

namespace ox {
class RayCast;

class Physics {
public:
  using EntityLayer = uint16_t;

  struct EntityLayerData {
    std::string name = "Layer";
    EntityLayer flags = 0xFFFF;
    uint8_t index = 1;
  };

  static std::map<EntityLayer, EntityLayerData> layer_collision_mask;

  static constexpr uint32_t MAX_BODIES = 1024;
  static constexpr uint32_t MAX_BODY_PAIRS = 1024;
  static constexpr uint32_t MAX_CONTACT_CONSTRAINS = 1024;
  static BPLayerInterfaceImpl layer_interface;
  static ObjectVsBroadPhaseLayerFilterImpl object_vs_broad_phase_layer_filter_interface;
  static ObjectLayerPairFilterImpl object_layer_pair_filter_interface;

  static void init();
  static void step(float physicsTs);
  static void shutdown();

  static JPH::PhysicsSystem* get_physics_system();
  static JPH::BodyInterface& get_body_interface();
  static const JPH::BroadPhaseQuery& get_broad_phase();

  static JPH::AllHitCollisionCollector<JPH::RayCastBodyCollector> cast_ray(const RayCast& ray_cast);

private:
  static JPH::PhysicsSystem* physics_system;
  static JPH::TempAllocatorImpl* temp_allocator;
  static JPH::JobSystemThreadPool* job_system;
};
}
