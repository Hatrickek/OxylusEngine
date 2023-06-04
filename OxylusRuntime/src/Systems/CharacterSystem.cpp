#include "CharacterSystem.h"

#include "Core/Components.h"
#include "Scene/Scene.h"
#include "UI/IGUI.h"

namespace OxylusRuntime {
  using namespace Oxylus;

  CharacterSystem::~CharacterSystem() { }

  void CharacterSystem::OnInit() { }

  void CharacterSystem::OnUpdate(Scene* scene, Timestep deltaTime) {
    auto& registery = scene->m_Registry;
    const auto characterView = registery.view<TransformComponent, CharacterControllerComponent>();
    const auto cameraView = registery.view<TransformComponent, CameraComponent>();

    for (const auto cameraEntity : cameraView) {
      auto&& [cameraTransform, cameraComponent] = cameraView.get<TransformComponent, CameraComponent>(cameraEntity);
      for (const auto entity : characterView) {
        auto&& [transform, component] = characterView.get<TransformComponent, CharacterControllerComponent>(entity);

        Vec3 controlInput = {};
        if (Input::GetKeyDown(Key::W))
          controlInput.x = -1;
        if (Input::GetKeyDown(Key::A))
          controlInput.z = -1;
        if (Input::GetKeyDown(Key::S))
          controlInput.x = 1;
        if (Input::GetKeyDown(Key::D))
          controlInput.z = 1;

        if (controlInput != Vec3(0))
          controlInput = normalize(controlInput);

        JPH::Vec3 cam_fwd = {cameraComponent.System->GetFront().x, cameraComponent.System->GetFront().y, cameraComponent.System->GetFront().z};
        cam_fwd.SetY(0.0f);
        cam_fwd = cam_fwd.NormalizedOr(JPH::Vec3::sAxisX());
        JPH::Quat rotation = JPH::Quat::sFromTo(JPH::Vec3::sAxisX(), cam_fwd);
        controlInput = glm::make_quat(rotation.GetXYZW().mF32) * controlInput;

        //controlInput = controlInput * cameraComponent.System->GetFront();

        bool jump = false;
        if (Input::GetKeyDown(Key::Space))
          jump = true;

        HandleInput(component, {controlInput.x, controlInput.y, controlInput.z}, jump, false, deltaTime);
      }
    }
  }

  void CharacterSystem::OnImGuiRender(Scene* scene, Timestep deltaTime) {
    auto& registery = scene->m_Registry;
    const auto characterView = registery.view<TransformComponent, CharacterControllerComponent>();
    CharacterControllerComponent* controllerComponent = nullptr;
    TransformComponent* controllerTransform = nullptr;
    for (const auto entity : characterView) {
      auto&& [transform, component] = characterView.get<TransformComponent, CharacterControllerComponent>(entity);
      controllerComponent = &component;
      controllerTransform = &transform;
    }

    if (controllerComponent) {
      if (ImGui::Begin("Character Debug")) {
        IGUI::BeginProperties();
        IGUI::DrawVec3Control("Position", controllerTransform->Translation);
        IGUI::Property("ControlMovementDuringJump", controllerComponent->ControlMovementDuringJump);
        IGUI::Property("CharacterSpeed", controllerComponent->CharacterSpeed, 1.0f, 10.0f);
        IGUI::Property("JumpSpeed", controllerComponent->JumpSpeed, 1.0f, 10.0f);
        IGUI::Property("CollisionTolerance", controllerComponent->CollisionTolerance, 0.05f, 0.5f);
        auto velocity = glm::make_vec3(controllerComponent->Character->GetLinearVelocity().mF32);
        IGUI::PropertyVector("Velocity", velocity);
        IGUI::EndProperties();
        ImGui::End();
      }
    }
  }

  void CharacterSystem::HandleInput(CharacterControllerComponent& characterComponent,
                                    JPH::Vec3Arg movementDirection,
                                    bool jump,
                                    bool switchStance,
                                    float deltaTime) {
    auto character = characterComponent.Character;
    // Cancel movement in opposite direction of normal when touching something we can't walk up
    JPH::Character::EGroundState ground_state = character->GetGroundState();
    if (ground_state == JPH::Character::EGroundState::OnSteepGround
        || ground_state == JPH::Character::EGroundState::NotSupported) {
      JPH::Vec3 normal = character->GetGroundNormal();
      normal.SetY(0.0f);
      const float dot = normal.Dot(movementDirection);
      if (dot < 0.0f)
        movementDirection -= (dot * normal) / normal.LengthSq();
    }

    // TODO(hatrickek): Crouching
    // Stance switch
    //if (switchStance)
    //  character->SetShape(
    //    character->GetShape() == character.mStandingShape ? mCrouchingShape : mStandingShape,
    //    1.5f * mPhysicsSystem->GetPhysicsSettings().mPenetrationSlop);

    if (characterComponent.ControlMovementDuringJump || character->IsSupported()) {
      // Update velocity
      JPH::Vec3 current_velocity = character->GetLinearVelocity();
      JPH::Vec3 desired_velocity = characterComponent.CharacterSpeed * movementDirection;
      desired_velocity.SetY(current_velocity.GetY());
      JPH::Vec3 new_velocity = 0.75f * current_velocity + 0.25f * desired_velocity;

      // Jump
      if (jump && ground_state == JPH::Character::EGroundState::OnGround)
        new_velocity += JPH::Vec3(0, characterComponent.JumpSpeed, 0);

      // Update the velocity
      character->SetLinearVelocity(new_velocity);
    }
  }
}
