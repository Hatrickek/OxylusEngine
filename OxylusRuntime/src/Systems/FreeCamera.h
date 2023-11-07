#pragma once
#include "Core/Base.h"
#include "Core/Systems/System.h"
#include "Render/Camera.h"

namespace OxylusRuntime {
  class FreeCamera : public Oxylus::System {
  public:
    void on_init() override;
    void on_update(Oxylus::Scene* scene, const Oxylus::Timestep& deltaTime) override;

  private:
    //Camera
    float m_TranslationDampening = 0.6f;
    float m_RotationDampening = 0.3f;
    bool m_SmoothCamera = true;
    float m_MouseSensitivity = 0.5f;
    float m_MovementSpeed = 5.0f;
    bool m_UseEditorCamera = true;
    bool m_UsingEditorCamera = false;
    glm::vec2 m_LockedMousePosition = glm::vec2(0.0f);
    glm::vec3 m_TranslationVelocity = glm::vec3(0);
    glm::vec2 m_RotationVelocity = glm::vec2(0);
  };
}
