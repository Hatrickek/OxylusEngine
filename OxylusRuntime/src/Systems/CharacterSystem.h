#pragma once
#include "Core/Components.h"
#include "Core/Types.h"
#include "Core/Systems/System.h"

namespace OxylusRuntime {

  class CharacterSystem : public Oxylus::System {
  public:
    ~CharacterSystem() override;
    void OnInit() override;
    void OnUpdate(Oxylus::Scene* scene, Oxylus::Timestep deltaTime) override;
    void OnImGuiRender(Oxylus::Scene* scene, Oxylus::Timestep deltaTime) override;

  private:
    void HandleInput(Oxylus::CharacterControllerComponent& characterComponent,
                     JPH::Vec3Arg inMovementDirection,
                     bool inJump,
                     bool inSwitchStance,
                     float inDeltaTime);
  };
}
