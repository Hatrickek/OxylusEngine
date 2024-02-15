#include "LuaPhysicsBindings.h"

#include <sol/state.hpp>
#include "LuaHelpers.h"

#include "Physics/JoltBuild.h"

#include "Jolt/Physics/Collision/CastResult.h"

#include "Physics/RayCast.h"

#include "Scene/Entity.h"

namespace Ox {
void LuaBindings::bind_physics(const Shared<sol::state>& state) {
  auto raycast_type = state->new_usertype<RayCast>("RayCast", sol::constructors<RayCast(Vec3, Vec3)>());
  SET_TYPE_FUNCTION(raycast_type, RayCast, get_point_on_ray);
  SET_TYPE_FUNCTION(raycast_type, RayCast, get_direction);
  SET_TYPE_FUNCTION(raycast_type, RayCast, get_origin);

  auto collector_type = state->new_usertype<JPH::AllHitCollisionCollector<JPH::RayCastBodyCollector>>("RayCastCollector");
  collector_type.set_function("had_hit", &JPH::AllHitCollisionCollector<JPH::RayCastBodyCollector>::HadHit);
  collector_type.set_function("sort", &JPH::AllHitCollisionCollector<JPH::RayCastBodyCollector>::Sort);

  auto result_type = state->new_usertype<JPH::BroadPhaseCastResult>("RayCastResult");
  result_type["fraction"] = &JPH::BroadPhaseCastResult::mFraction;

  auto physics_table = state->create_table("Physics");
  physics_table.set_function("cast_ray", [](const RayCast& ray) -> JPH::AllHitCollisionCollector<JPH::RayCastBodyCollector> { return Physics::cast_ray(ray); });
  physics_table.set_function("get_hits",
                             [](const JPH::AllHitCollisionCollector<JPH::RayCastBodyCollector>& collector) -> std::vector<JPH::BroadPhaseCastResult> {
                               return {collector.mHits.begin(), collector.mHits.end()};
                             });

  // -- Components ---

  auto rigidody_component = state->new_usertype<RigidbodyComponent>("RigidbodyComponent");

  const std::initializer_list<std::pair<sol::string_view, RigidbodyComponent::BodyType>> rigidbody_body_type = {
    {"Static", RigidbodyComponent::BodyType::Static},
    {"Kinematic", RigidbodyComponent::BodyType::Kinematic},
    {"Dynamic", RigidbodyComponent::BodyType::Dynamic}
  };
  state->new_enum<RigidbodyComponent::BodyType, true>("RigidbodyBodyType", rigidbody_body_type);

  SET_TYPE_FIELD(rigidody_component, RigidbodyComponent, type);
  SET_TYPE_FIELD(rigidody_component, RigidbodyComponent, mass);
  SET_TYPE_FIELD(rigidody_component, RigidbodyComponent, linear_drag);
  SET_TYPE_FIELD(rigidody_component, RigidbodyComponent, angular_drag);
  SET_TYPE_FIELD(rigidody_component, RigidbodyComponent, gravity_scale);
  SET_TYPE_FIELD(rigidody_component, RigidbodyComponent, allow_sleep);
  SET_TYPE_FIELD(rigidody_component, RigidbodyComponent, awake);
  SET_TYPE_FIELD(rigidody_component, RigidbodyComponent, continuous);
  SET_TYPE_FIELD(rigidody_component, RigidbodyComponent, interpolation);
  SET_TYPE_FIELD(rigidody_component, RigidbodyComponent, is_sensor);
  SET_COMPONENT_TYPE_ID(rigidody_component, RigidbodyComponent);
  register_meta_component<RigidbodyComponent>();

  auto box_collider_component = state->new_usertype<BoxColliderComponent>("BoxColliderComponent");
  SET_TYPE_FIELD(box_collider_component, BoxColliderComponent, size);
  SET_TYPE_FIELD(box_collider_component, BoxColliderComponent, offset);
  SET_TYPE_FIELD(box_collider_component, BoxColliderComponent, density);
  SET_TYPE_FIELD(box_collider_component, BoxColliderComponent, friction);
  SET_TYPE_FIELD(box_collider_component, BoxColliderComponent, restitution);
  SET_COMPONENT_TYPE_ID(box_collider_component, BoxColliderComponent);
  register_meta_component<BoxColliderComponent>();

  auto sphere_collider_component = state->new_usertype<SphereColliderComponent>("SphereColliderComponent");
  SET_TYPE_FIELD(sphere_collider_component, SphereColliderComponent, radius);
  SET_TYPE_FIELD(sphere_collider_component, SphereColliderComponent, offset);
  SET_TYPE_FIELD(sphere_collider_component, SphereColliderComponent, density);
  SET_TYPE_FIELD(sphere_collider_component, SphereColliderComponent, friction);
  SET_TYPE_FIELD(sphere_collider_component, SphereColliderComponent, restitution);
  SET_COMPONENT_TYPE_ID(sphere_collider_component, SphereColliderComponent);
  register_meta_component<SphereColliderComponent>();

  auto capsule_collider_component = state->new_usertype<CapsuleColliderComponent>("CapsuleColliderComponent");
  SET_TYPE_FIELD(capsule_collider_component, CapsuleColliderComponent, height);
  SET_TYPE_FIELD(capsule_collider_component, CapsuleColliderComponent, radius);
  SET_TYPE_FIELD(capsule_collider_component, CapsuleColliderComponent, offset);
  SET_TYPE_FIELD(capsule_collider_component, CapsuleColliderComponent, density);
  SET_TYPE_FIELD(capsule_collider_component, CapsuleColliderComponent, friction);
  SET_TYPE_FIELD(capsule_collider_component, CapsuleColliderComponent, restitution);
  SET_COMPONENT_TYPE_ID(capsule_collider_component, CapsuleColliderComponent);
  register_meta_component<CapsuleColliderComponent>();

  auto tapered_capsule_collider_component = state->new_usertype<TaperedCapsuleColliderComponent>("TaperedCapsuleColliderComponent");
  SET_TYPE_FIELD(tapered_capsule_collider_component, TaperedCapsuleColliderComponent, height);
  SET_TYPE_FIELD(tapered_capsule_collider_component, TaperedCapsuleColliderComponent, top_radius);
  SET_TYPE_FIELD(tapered_capsule_collider_component, TaperedCapsuleColliderComponent, bottom_radius);
  SET_TYPE_FIELD(tapered_capsule_collider_component, TaperedCapsuleColliderComponent, offset);
  SET_TYPE_FIELD(tapered_capsule_collider_component, TaperedCapsuleColliderComponent, density);
  SET_TYPE_FIELD(tapered_capsule_collider_component, TaperedCapsuleColliderComponent, friction);
  SET_TYPE_FIELD(tapered_capsule_collider_component, TaperedCapsuleColliderComponent, restitution);
  SET_COMPONENT_TYPE_ID(tapered_capsule_collider_component, TaperedCapsuleColliderComponent);
  register_meta_component<TaperedCapsuleColliderComponent>();

  auto cylinder_collider_component = state->new_usertype<CylinderColliderComponent>("CylinderColliderComponent");
  SET_TYPE_FIELD(cylinder_collider_component, CylinderColliderComponent, height);
  SET_TYPE_FIELD(cylinder_collider_component, CylinderColliderComponent, radius);
  SET_TYPE_FIELD(cylinder_collider_component, CylinderColliderComponent, offset);
  SET_TYPE_FIELD(cylinder_collider_component, CylinderColliderComponent, density);
  SET_TYPE_FIELD(cylinder_collider_component, CylinderColliderComponent, friction);
  SET_TYPE_FIELD(cylinder_collider_component, CylinderColliderComponent, restitution);
  SET_COMPONENT_TYPE_ID(cylinder_collider_component, CylinderColliderComponent);
  register_meta_component<CylinderColliderComponent>();

  auto mesh_collider_component = state->new_usertype<MeshColliderComponent>("MeshColliderComponent");
  SET_TYPE_FIELD(mesh_collider_component, MeshColliderComponent, offset);
  SET_TYPE_FIELD(mesh_collider_component, MeshColliderComponent, friction);
  SET_TYPE_FIELD(mesh_collider_component, MeshColliderComponent, restitution);
  SET_COMPONENT_TYPE_ID(mesh_collider_component, MeshColliderComponent);
  register_meta_component<MeshColliderComponent>();
}
}
