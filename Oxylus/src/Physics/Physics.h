#pragma once
#include <map>

#include "PhysicsInterfaces.h"

namespace Oxylus {
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
  static BPLayerInterfaceImpl s_layer_interface;
  static ObjectVsBroadPhaseLayerFilterImpl s_object_vs_broad_phase_layer_filter_interface;
  static ObjectLayerPairFilterImpl s_object_layer_pair_filter_interface;

  static void init();
  static void step(float physicsTs);
  static void shutdown();

  static JPH::PhysicsSystem* get_physics_system();
  static JPH::BodyInterface& get_body_interface();

private:
  static JPH::PhysicsSystem* s_PhysicsSystem;
  static JPH::TempAllocatorImpl* s_TempAllocator;
  static JPH::JobSystemThreadPool* s_JobSystem;
};
}
