#include "FreeCamera.h"
#include <imgui.h>

#include "Core/Components.h"
#include "Core/Input.h"
#include "Scene/Scene.h"
#include "Utils/OxMath.h"
#include "Utils/Timestep.h"

namespace OxylusRuntime {
  using namespace Oxylus;

  void FreeCamera::OnInit() { }

  void FreeCamera::OnUpdate(Scene* scene, Timestep deltaTime) {
    auto& registery = scene->m_Registry;
    const auto group = registery.view<TransformComponent, CameraComponent>();
    for (const auto entity : group) {
      auto&& [transform, component] = group.get<TransformComponent, CameraComponent>(entity);
      auto& camera = component.system;
      const glm::vec3& position = camera->GetPosition();
      const glm::vec2 yawPitch = glm::vec2(camera->GetYaw(), camera->GetPitch());
      glm::vec3 finalPosition = position;
      glm::vec2 finalYawPitch = yawPitch;

      if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);
        const glm::vec2 newMousePosition = Input::GetMousePosition();

        if (!m_UsingEditorCamera) {
          m_UsingEditorCamera = true;
          m_LockedMousePosition = newMousePosition;
        }

        Input::SetCursorPosition(m_LockedMousePosition.x, m_LockedMousePosition.y);

        const glm::vec2 change = (newMousePosition - m_LockedMousePosition) * m_MouseSensitivity;
        finalYawPitch.x += change.x;
        finalYawPitch.y = glm::clamp(finalYawPitch.y - change.y, -89.9f, 89.9f);

        const float maxMoveSpeed = m_MovementSpeed * (ImGui::IsKeyDown(ImGuiKey_LeftShift) ? 3.0f : 1.0f);
        if (ImGui::IsKeyDown(ImGuiKey_W))
          finalPosition += camera->GetFront() * maxMoveSpeed;
        else if (ImGui::IsKeyDown(ImGuiKey_S))
          finalPosition -= camera->GetFront() * maxMoveSpeed;
        if (ImGui::IsKeyDown(ImGuiKey_D))
          finalPosition += camera->GetRight() * maxMoveSpeed;
        else if (ImGui::IsKeyDown(ImGuiKey_A))
          finalPosition -= camera->GetRight() * maxMoveSpeed;

        if (ImGui::IsKeyDown(ImGuiKey_Q)) {
          finalPosition.y -= maxMoveSpeed;
        }
        else if (ImGui::IsKeyDown(ImGuiKey_E)) {
          finalPosition.y += maxMoveSpeed;
        }
      }
      else {
        m_UsingEditorCamera = false;
      }

      const glm::vec3 dampedPosition = Math::smooth_damp(position,
        finalPosition,
        m_TranslationVelocity,
        m_TranslationDampening,
        10000.0f,
        deltaTime);
      const glm::vec2 dampedYawPitch = Math::smooth_damp(yawPitch,
        finalYawPitch,
        m_RotationVelocity,
        m_RotationDampening,
        1000.0f,
        deltaTime);
      
      transform.translation = m_SmoothCamera ? dampedPosition : finalPosition;
      transform.rotation.x = m_SmoothCamera ? dampedYawPitch.y : finalYawPitch.y;
      transform.rotation.y = m_SmoothCamera ? dampedYawPitch.x : finalYawPitch.x;
    }
  }
}
