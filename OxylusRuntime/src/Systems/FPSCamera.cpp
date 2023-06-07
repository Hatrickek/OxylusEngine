#include "FPSCamera.h"

#include "Core/Components.h"
#include "Scene/Scene.h"
#include "UI/IGUI.h"

namespace OxylusRuntime {
  using namespace Oxylus;

  FPSCamera::~FPSCamera() = default;

  void FPSCamera::OnInit() { }

  void FPSCamera::OnUpdate(Scene* scene, Timestep deltaTime) {
    const auto cameraView = scene->m_Registry.view<TransformComponent, CameraComponent>();
    const auto charView = scene->m_Registry.view<TransformComponent, CharacterControllerComponent>();

    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
      m_LockCamera = !m_LockCamera;

    for (const auto charEntity : charView) {
      auto&& [charTransform, charComponent] = charView.get<TransformComponent, CharacterControllerComponent>(charEntity);
      for (const auto entity : cameraView) {
        auto&& [transform, component] = cameraView.get<TransformComponent, CameraComponent>(entity);
        const auto& camera = component.System;
        const Vec2 yawPitch = Vec2(camera->GetYaw(), camera->GetPitch());
        Vec2 finalYawPitch = yawPitch;

        if (m_LockCamera) {
          ImGui::SetMouseCursor(ImGuiMouseCursor_None);
          const Vec2 newMousePosition = Input::GetMousePosition();

          if (!m_UsingCamera) {
            m_UsingCamera = true;
            m_LockedMousePosition = newMousePosition;
          }

          Input::SetCursorPosition(m_LockedMousePosition.x, m_LockedMousePosition.y);

          const Vec2 change = (newMousePosition - m_LockedMousePosition) * m_MouseSensitivity * (float)deltaTime;
          finalYawPitch.x += change.x;
          finalYawPitch.y = glm::clamp(finalYawPitch.y - change.y, -89.9f, 89.9f);
        }
        else {
          m_UsingCamera = false;
        }

        Vec3 finalPosition = charTransform.Translation;
        finalPosition.y += charComponent.CharacterHeightStanding;

        transform.Translation = finalPosition;
        transform.Rotation.x = finalYawPitch.y;
        transform.Rotation.y = finalYawPitch.x;
      }
    }
  }

  void FPSCamera::OnImGuiRender(Scene* scene, Timestep deltaTime) {
    const auto cameraView = scene->m_Registry.view<TransformComponent, CameraComponent>();
    const CameraComponent* cameraComponent = nullptr;
    for (const auto entity : cameraView) {
      auto&& [transform, component] = cameraView.get<TransformComponent, CameraComponent>(entity);
      cameraComponent = &component;
    }
    if (ImGui::Begin("Camera Debug")) {
      IGUI::BeginProperties();
      if (IGUI::Property("FOV", cameraComponent->System->Fov)) {
        cameraComponent->System->SetFov(cameraComponent->System->Fov);
      }
      IGUI::Property("Sensitivity", m_MouseSensitivity);
      IGUI::EndProperties();

      ImGui::End();
    }
  }
}
