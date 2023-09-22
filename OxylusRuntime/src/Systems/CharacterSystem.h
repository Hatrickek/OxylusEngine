#pragma once
#include "Core/Components.h"
#include "Core/Systems/System.h"

namespace OxylusRuntime {
  class CharacterSystem : public Oxylus::System {
  public:
    ~CharacterSystem() override;
    void OnInit() override;
    void OnUpdate(Oxylus::Scene* scene, Oxylus::Timestep deltaTime) override;
    void OnImGuiRender(Oxylus::Scene* scene, Oxylus::Timestep deltaTime) override;
    void OnContactAdded(Oxylus::Scene* scene, const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, const JPH::ContactSettings& settings) override;

  private:
    struct MovementArgs {
      JPH::Vec3 WishDir = {};
      JPH::Vec3 MovementDirection = {};
      Oxylus::CharacterControllerComponent CharacterController;
      bool JumpQueued = {};
      bool IsGrounded = {};
    };

    static bool QueueJump(bool autoBunnyHop);
    static void GroundMove(JPH::Vec3& velocity, const MovementArgs& args, float deltaTime);
    static void AirControl(JPH::Vec3& velocity, JPH::Vec3 targetDir, JPH::Vec3 movInput, float targetSpeed, float airControl);
    static void AirMove(JPH::Vec3& velocity, const MovementArgs& args, float deltaTime);
    static void ApplyFriction(JPH::Vec3& velocity, float groundDeceleration, bool isGrounded, float friction, float deltaTime, float t);
    static void Accelerate(JPH::Vec3& velocity, const JPH::Vec3& targetDir, float targetSpeed, float accel);
  };
}
