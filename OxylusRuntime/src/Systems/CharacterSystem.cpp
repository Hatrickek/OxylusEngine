#include "CharacterSystem.h"

#include "Core/Components.h"
#include "Scene/Scene.h"
#include "UI/IGUI.h"

namespace OxylusRuntime {
  using namespace Oxylus;

  // Temporary globals
  static float s_MaxHorizontalVelocity = 0.0f;
  static float s_MaxVerticalVelocity = 0.0f;

  CharacterSystem::~CharacterSystem() = default;

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

        JPH::Vec3 camFwd = {cameraComponent.System->GetFront().x, cameraComponent.System->GetFront().y, cameraComponent.System->GetFront().z};
        camFwd.SetY(0.0f);
        camFwd = camFwd.NormalizedOr(JPH::Vec3::sAxisX());
        JPH::Quat rotation = JPH::Quat::sFromTo(JPH::Vec3::sAxisX(), camFwd);
        controlInput = glm::make_quat(rotation.GetXYZW().mF32) * controlInput;

        //controlInput = controlInput * cameraComponent.System->GetFront();

        bool jump = false;
        if (ImGui::IsKeyPressed(ImGuiKey_Space, false))
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
        IGUI::PropertyVector("Position", controllerTransform->Translation);
        IGUI::Property("ControlMovementDuringJump", controllerComponent->ControlMovementDuringJump);
        IGUI::Property("CharacterSpeed", controllerComponent->CharacterSpeed, 1.0f, 10.0f);
        IGUI::Property("JumpSpeed", controllerComponent->JumpSpeed, 1.0f, 10.0f);
        IGUI::Property("CollisionTolerance", controllerComponent->CollisionTolerance, 0.05f, 0.5f);
        IGUI::EndProperties();
        
        auto velocity = glm::make_vec3(controllerComponent->Character->GetLinearVelocity().mF32);
        IGUI::BeginProperties();
        IGUI::PropertyVector("Velocity", velocity);
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

  void CharacterSystem::HandleInput(const CharacterControllerComponent& characterComponent,
                                    JPH::Vec3Arg movementDirection,
                                    bool jump,
                                    bool switchStance,
                                    float deltaTime) {
    auto& character = characterComponent.Character;
    // Cancel movement in opposite direction of normal when touching something we can't walk up
    const JPH::Character::EGroundState groundState = character->GetGroundState();
    if (groundState == JPH::Character::EGroundState::OnSteepGround
        || groundState == JPH::Character::EGroundState::NotSupported) {
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
      const JPH::Vec3 currentVelocity = character->GetLinearVelocity();
      JPH::Vec3 desiredVelocity = characterComponent.CharacterSpeed * movementDirection;
      desiredVelocity.SetY(currentVelocity.GetY());
      JPH::Vec3 newVelocity = 0.75f * currentVelocity + 0.25f * desiredVelocity;

      // Jump
      if (jump && groundState == JPH::Character::EGroundState::OnGround)
        newVelocity += JPH::Vec3(0, characterComponent.JumpSpeed, 0);

      // Update the velocity
      character->SetLinearVelocity(newVelocity);
    }
  }
}
