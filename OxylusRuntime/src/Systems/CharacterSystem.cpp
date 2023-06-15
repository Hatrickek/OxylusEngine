#include "CharacterSystem.h"

#include "Core/Components.h"
#include "Scene/Scene.h"
#include "UI/IGUI.h"

namespace OxylusRuntime {
  using namespace Oxylus;

  // Temporary globals
  static float s_MaxHorizontalVelocity = 0.0f;
  static float s_MaxVerticalVelocity = 0.0f;

  static bool s_LockCamera = false;
  static Vec2 s_LockedMousePosition = {};
  static float s_MouseSensitivity = 1.0f;
  static bool s_UsingCamera = false;

  CharacterSystem::~CharacterSystem() = default;

  void CharacterSystem::OnInit() { }

  void CharacterSystem::OnUpdate(Scene* scene, Timestep deltaTime) {
    auto& registery = scene->m_Registry;
    const auto characterView = registery.view<TransformComponent, CharacterControllerComponent>();
    const auto cameraView = registery.view<TransformComponent, CameraComponent>();

    for (const auto cameraEntity : cameraView) {
      auto&& [cameraTransform, cameraComponent] = cameraView.get<TransformComponent, CameraComponent>(cameraEntity);
      for (const auto entity : characterView) {
        auto&& [chTransform, chComponent] = characterView.get<TransformComponent, CharacterControllerComponent>(entity);

        JPH::Vec3 wishDir = {};
        JPH::Vec3 movementDir = {};
        if (Input::GetKeyDown(Key::W))
          movementDir.SetX(1);
        if (Input::GetKeyDown(Key::A))
          movementDir.SetZ(-1);
        if (Input::GetKeyDown(Key::S))
          movementDir.SetX(-1);
        if (Input::GetKeyDown(Key::D))
          movementDir.SetZ(1);

        wishDir = movementDir;

        if (wishDir != JPH::Vec3::sZero())
          wishDir = wishDir.Normalized();

        JPH::Vec3 camFwd = {cameraComponent.System->GetFront().x, cameraComponent.System->GetFront().y, cameraComponent.System->GetFront().z};
        camFwd.SetY(0.0f);
        camFwd = camFwd.NormalizedOr(JPH::Vec3::sAxisX());
        JPH::Quat rotation = JPH::Quat::sFromTo(JPH::Vec3::sAxisX(), camFwd);
        wishDir = rotation * wishDir;

        bool jumpQueued = QueueJump(chComponent.AutoBunnyHop);

        const bool isGrounded = chComponent.Character->IsSupported();

        // Set movement state.
        MovementArgs args{
          .WishDir = wishDir,
          .MovementDirection = movementDir,
          .CharacterController = chComponent,
          .JumpQueued = jumpQueued,
          .IsGrounded = isGrounded
        };

        if (isGrounded) {
          GroundMove(chComponent.CurrentVelocity, args, deltaTime);
          jumpQueued = false;
        }
        else {
          AirMove(chComponent.CurrentVelocity, args, deltaTime);
        }

        // Rotate the character and camera.

        const auto& camera = cameraComponent.System;
        const Vec2 yawPitch = Vec2(camera->GetYaw(), camera->GetPitch());
        Vec2 finalYawPitch = yawPitch;

        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
          s_LockCamera = !s_LockCamera;

        if (s_LockCamera) {
          ImGui::SetMouseCursor(ImGuiMouseCursor_None);
          const Vec2 newMousePosition = Input::GetMousePosition();

          if (!s_UsingCamera) {
            s_UsingCamera = true;
            s_LockedMousePosition = newMousePosition;
          }

          Input::SetCursorPosition(s_LockedMousePosition.x, s_LockedMousePosition.y);

          const Vec2 change = (newMousePosition - s_LockedMousePosition) * (s_MouseSensitivity / 1000.0f);
          finalYawPitch.x += change.x;
          finalYawPitch.y = glm::clamp(finalYawPitch.y - change.y, -89.0f, 89.9f);
        }
        else {
          s_UsingCamera = false;
        }

        auto charRotation = JPH::Quat::sEulerAngles({0.0f, -finalYawPitch.x, 0.0f});
        chComponent.Character->SetRotation(charRotation);

        Vec3 finalPosition = chTransform.Translation;
        finalPosition.y += chComponent.CharacterHeightStanding;

        cameraTransform.Translation = finalPosition;
        cameraTransform.Rotation.x = finalYawPitch.y;
        cameraTransform.Rotation.y = finalYawPitch.x;
        
        // Cancel movement in opposite direction of normal when touching something we can't walk up
        const JPH::Character::EGroundState groundState = chComponent.Character->GetGroundState();
        if (groundState == JPH::Character::EGroundState::OnSteepGround || groundState == JPH::Character::EGroundState::NotSupported) {
          JPH::Vec3 normal = chComponent.Character->GetGroundNormal();
          normal.SetY(0.0f);
          const float dot = normal.Dot(chComponent.CurrentVelocity);
          if (dot < 0.0f)
            chComponent.CurrentVelocity -= (dot * normal) / normal.LengthSq();
        }

        // Move the character.
        chComponent.Character->SetLinearVelocity(chComponent.CurrentVelocity);
      }
    }
  }

  void CharacterSystem::OnImGuiRender(Scene* scene, Timestep deltaTime) {
    auto& registery = scene->m_Registry;
    const auto characterView = registery.view<TransformComponent, CharacterControllerComponent>();
    for (const auto entity : characterView) {
      auto&& [transform, component] = characterView.get<TransformComponent, CharacterControllerComponent>(entity);
      
      if (ImGui::Begin("Character Debug")) {
        IGUI::BeginProperties();
        IGUI::PropertyVector("Position", transform.Translation);
        IGUI::PropertyVector("Rotation", transform.Rotation);
        IGUI::Property("AutoBunnyHop", component.AutoBunnyHop);
        IGUI::Property("JumpForce", component.JumpForce, 1.0f, 10.0f);
        IGUI::Property("CollisionTolerance", component.CollisionTolerance, 0.05f, 0.5f);

        auto velocity = glm::make_vec3(component.Character->GetLinearVelocity().mF32);
        auto speed = component.Character->GetLinearVelocity().Length();
        IGUI::Text("IsGrounded", fmt::format("{}", component.Character->IsSupported()).c_str());
        IGUI::PropertyVector("Velocity", velocity);
        IGUI::Property("Speed", speed);
        if (s_MaxVerticalVelocity <= velocity.y)
          s_MaxVerticalVelocity = velocity.y;
        IGUI::Text("Max Vertical Velocity", fmt::format("{}", s_MaxVerticalVelocity).c_str());
        if (s_MaxHorizontalVelocity <= velocity.x)
          s_MaxHorizontalVelocity = velocity.x;
        IGUI::Text("Max Horizontal Velocity", fmt::format("{}", s_MaxHorizontalVelocity).c_str());
        IGUI::EndProperties();

        ImGui::End();
      }
    }
  }

  void CharacterSystem::OnContactAdded(Scene* scene, const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, const JPH::ContactSettings& settings) {
    auto& registery = scene->m_Registry;
    const auto characterView = registery.view<TransformComponent, CharacterControllerComponent>();
    const auto rbView = registery.view<TagComponent, TransformComponent, RigidbodyComponent>();

    for (const auto rbEntity : rbView) {
      auto&& [tag, rbTransform, rb] = rbView.get<TagComponent, TransformComponent, RigidbodyComponent>(rbEntity);

      const auto& id = ((JPH::Body*)rb.RuntimeBody)->GetID();

      if (!body1.IsSensor() || id != body1.GetID()) {
        continue;
      }

      if (tag.Tag == "BouncePad") {
        for (const auto chEntity : characterView) {
          auto&& [chTransform, ch] = characterView.get<TransformComponent, CharacterControllerComponent>(chEntity);
          if (body2.GetID() == ch.Character->GetBodyID()) {
            ch.Character->SetLinearVelocity(JPH::Vec3{0.0f, 12.0f, 0.0f}, false);
          }
        }
      }
      else if (tag.Tag == "AccelPad") {
        for (const auto chEntity : characterView) {
          auto&& [chTransform, ch] = characterView.get<TransformComponent, CharacterControllerComponent>(chEntity);
          if (body2.GetID() == ch.Character->GetBodyID()) {
            ch.Character->SetLinearVelocity(JPH::Vec3(rb.Translation.x, rb.Translation.y, rb.Translation.z) * 30, false);
          }
        }
      }
    }
  }

  bool CharacterSystem::QueueJump(const bool autoBunnyHop) {
    bool jumpQueued = false;
    if (autoBunnyHop) {
      jumpQueued = Input::GetKeyDown(Key::Space);
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Space, false) && !jumpQueued) {
      jumpQueued = true;
    }

    if (ImGui::IsKeyReleased(ImGuiKey_Space)) {
      jumpQueued = false;
    }

    return jumpQueued;
  }

  void CharacterSystem::GroundMove(JPH::Vec3& velocity, const MovementArgs& args, float deltaTime) {
    // Do not apply friction if the player is queueing up the next jump
    if (!args.JumpQueued) {
      ApplyFriction(velocity, args.CharacterController.GroundSettings.Deceleration, args.IsGrounded, args.CharacterController.Friction, deltaTime, 1.0f);
    }
    else {
      ApplyFriction(velocity, args.CharacterController.GroundSettings.Deceleration, args.IsGrounded, args.CharacterController.Friction, deltaTime, 0.0f);
    }

    auto wishspeed = args.WishDir.Length();
    wishspeed *= args.CharacterController.GroundSettings.MaxSpeed;

    Accelerate(velocity, args.WishDir, wishspeed, args.CharacterController.GroundSettings.Acceleration);

    // Reset the gravity velocity
    velocity.SetY(-args.CharacterController.Gravity * deltaTime);

    if (args.JumpQueued) {
      velocity.SetY(args.CharacterController.JumpForce);
    }
  }

  void CharacterSystem::AirMove(JPH::Vec3& velocity, const MovementArgs& args, float deltaTime) {
    float accel;

    float wishspeed = args.WishDir.Length();
    wishspeed *= args.CharacterController.AirSettings.MaxSpeed;

    // CPM Air control.
    const float wishspeed2 = wishspeed;
    if (velocity.Dot(args.WishDir) < 0) {
      accel = args.CharacterController.AirSettings.Deceleration;
    }
    else {
      accel = args.CharacterController.AirSettings.Acceleration;
    }

    // If the player is ONLY strafing left or right
    if (args.MovementDirection.GetZ() == 0.0f && args.MovementDirection.GetX() != 0.0f) {
      if (wishspeed > args.CharacterController.StrafeSettings.MaxSpeed) {
        wishspeed = args.CharacterController.StrafeSettings.MaxSpeed;
      }

      accel = args.CharacterController.StrafeSettings.Acceleration;
    }

    Accelerate(velocity, args.WishDir, wishspeed, accel);
    /*if (args.CharacterController.AirControl > 0) {
      AirControl(velocity, args.WishDir, args.WishDir, wishspeed2, args.CharacterController.AirControl);
    }*/

    // Apply gravity
    velocity.SetY(velocity.GetY() - args.CharacterController.Gravity * deltaTime);
  }

  void CharacterSystem::AirControl(JPH::Vec3& velocity, JPH::Vec3 targetDir, JPH::Vec3 movInput, float targetSpeed, float airControl) {
    // Only control air movement when moving forward or backward.
    if (glm::abs(targetDir.GetZ()) < 0.001f || glm::abs(targetSpeed) < 0.001f) {
      return;
    }

    const float zSpeed = velocity.GetY();
    velocity.SetY(0);
    /* Next two lines are equivalent to idTech's VectorNormalize() */
    const float speed = velocity.Length();
    velocity = velocity.Normalized();

    const float dot = velocity.Dot(targetDir);
    float k = 32;
    k *= airControl * dot * dot * Application::GetTimestep();

    // Change direction while slowing down.
    if (dot > 0) {
      velocity.SetX(velocity.GetX() * speed + targetDir.GetX() * k);
      velocity.SetY(velocity.GetY() * speed + targetDir.GetY() * k);
      velocity.SetZ(velocity.GetZ() * speed + targetDir.GetZ() * k);

      velocity = velocity.Normalized();
    }

    velocity.SetX(velocity.GetX() * speed);
    velocity.SetY(zSpeed); // Note this line
    velocity.SetZ(velocity.GetZ() * speed);
  }

  void CharacterSystem::ApplyFriction(JPH::Vec3& velocity, float groundDeceleration, bool isGrounded, float friction, float deltaTime, float t) {
    JPH::Vec3 vec = velocity;
    vec.SetY(0.0f);
    const float speed = vec.Length();
    float drop = 0;

    // Only apply friction when grounded.
    if (isGrounded) {
      const float control = speed < groundDeceleration ? groundDeceleration : speed;
      drop = control * friction * deltaTime * t;
    }

    float newSpeed = speed - drop;
    // m_PlayerFriction = newSpeed; // For logging
    if (newSpeed < 0) {
      newSpeed = 0;
    }

    if (speed > 0) {
      newSpeed /= speed;
    }

    velocity.SetX(velocity.GetX() * newSpeed);
    velocity.SetZ(velocity.GetZ() * newSpeed);
  }

  void CharacterSystem::Accelerate(JPH::Vec3& velocity, const JPH::Vec3& targetDir, float targetSpeed, float accel) {
    const float currentspeed = velocity.Dot(targetDir);
    const float addspeed = targetSpeed - currentspeed;
    if (addspeed <= 0) {
      return;
    }

    float accelspeed = accel * Application::GetTimestep() * targetSpeed;
    if (accelspeed > addspeed) {
      accelspeed = addspeed;
    }

    velocity.SetX(velocity.GetX() + accelspeed * targetDir.GetX());
    velocity.SetZ(velocity.GetZ() + accelspeed * targetDir.GetZ());
  }
}
