#pragma once
#include "Core/Types.h"
#include "Core/Systems/System.h"

namespace OxylusRuntime {
  class FPSCamera : public Oxylus::System {
  public:
    ~FPSCamera() override;
    void OnInit() override;
    void OnUpdate(Oxylus::Scene* scene, Oxylus::Timestep deltaTime) override;
    void OnImGuiRender(Oxylus::Scene* scene, Oxylus::Timestep deltaTime) override;
  private:
    float m_MouseSensitivity = 1.0f;
    bool m_LockCamera = false;
    Oxylus::Vec2 m_LockedMousePosition = Oxylus::Vec2(800, 450.0f);
    bool m_UsingCamera = false;
  };
}
