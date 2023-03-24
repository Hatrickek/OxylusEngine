#include "ViewportPanel.h"

#include <icons/IconsMaterialDesignIcons.h>

#include "EditorLayer.h"
#include "ImGuizmo.h"
#include "glm/gtc/type_ptr.hpp"
#include "Render/Vulkan/VulkanRenderer.h"
#include "Scene/EntitySerializer.h"
#include "UI/IGUI.h"
#include "Utils/OxMath.h"
#include "utils/timestep.h"

namespace Oxylus {
  ViewportPanel::ViewportPanel() : EditorPanel("Viewport", ICON_MDI_TERRAIN, true) {
    VulkanRenderer::SetCamera(Camera);
  }

  void ViewportPanel::OnImGuiRender() {
    DrawPerformanceOverlay();

    bool viewportSettingsPopup = false;
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});

    if (OnBegin(flags)) {
      constexpr auto popupItemSpacing = ImVec2(6.0f, 8.0f); //TODO: Get this from theme
      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, popupItemSpacing);
      if (ImGui::BeginPopupContextItem("RightClick")) {
        if (ImGui::MenuItem("Fullscreen"))
          FullscreenViewport = !FullscreenViewport;
        ImGui::EndPopup();
      }
      ImGui::PopStyleVar();

      if (ImGui::BeginMenuBar()) {
        if (ImGui::MenuItem(StringUtils::FromChar8T(ICON_MDI_COGS))) {
          viewportSettingsPopup = true;
        }
        ImGui::EndMenuBar();
      }
      ImGui::PopStyleVar();

      if (viewportSettingsPopup)
        ImGui::OpenPopup("ViewportSettings");

      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
      if (ImGui::BeginPopup("ViewportSettings")) {
        IGUI::BeginProperties();
        static bool vsync = VulkanRenderer::SwapChain.m_PresentMode == vk::PresentModeKHR::eFifo ? true : false;
        if (IGUI::Property("VSync", vsync)) {
          vsync = VulkanRenderer::SwapChain.ToggleVsync();
          RendererConfig::Get()->DisplayConfig.VSync = vsync;
        }
        IGUI::Property<float>("Camera sensitivity", m_MouseSensitivity, 0.0f, 20.0f);
        IGUI::Property<float>("Movement speed", m_MovementSpeed, 30.0f, 500.0f);
        IGUI::Property("Smooth camera", m_SmoothCamera);
        IGUI::EndProperties();
        ImGui::EndPopup();
      }

      const auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
      const auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
      m_ViewportOffset = ImGui::GetWindowPos();
      m_ViewportBounds[0] = {viewportMinRegion.x + m_ViewportOffset.x, viewportMinRegion.y + m_ViewportOffset.y};
      m_ViewportBounds[1] = {viewportMaxRegion.x + m_ViewportOffset.x, viewportMaxRegion.y + m_ViewportOffset.y};

      m_ViewportFocused = ImGui::IsWindowFocused();
      m_ViewportHovered = ImGui::IsWindowHovered();

      m_ViewportPanelSize = ImGui::GetContentRegionAvail();
      VulkanRenderer::s_RendererContext.ViewportSize = m_ViewportPanelSize;
      if (m_ViewportSize.x != m_ViewportPanelSize.x || m_ViewportSize.y != m_ViewportPanelSize.y) {
        //VulkanRenderer::OnResize(); TODO: Currently causes lags while resizing viewport.
        m_ViewportSize = {m_ViewportPanelSize.x, m_ViewportPanelSize.y};
      }
      ImGui::Image(VulkanRenderer::GetFinalImage().GetDescriptorSet(), ImVec2{m_ViewportSize.x, m_ViewportSize.y});

      if (m_SceneHierarchyPanel)
        m_SceneHierarchyPanel->DragDropTarget();

      DrawGizmos();

      ImGui::PopStyleVar();
      OnEnd();
    }
  }

  void ViewportPanel::SetContext(const Ref<Scene>& scene, SceneHierarchyPanel& sceneHierarchyPanel) {
    m_SceneHierarchyPanel = &sceneHierarchyPanel;
    m_Scene = scene;
  }

  void ViewportPanel::OnUpdate() {
    if (m_ViewportFocused && !m_SimulationRunning && m_UseEditorCamera) {
      const glm::vec3& position = Camera.GetPosition();
      const glm::vec2 yawPitch = glm::vec2(Camera.GetYaw(), Camera.GetPitch());
      glm::vec3 finalPosition = position;
      glm::vec2 finalYawPitch = yawPitch;

      if (ImGui::IsMouseDown(ImGuiMouseButton_Right) && Camera.ShouldMove) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);
        const glm::vec2 newMousePosition = Input::GetMousePosition();

        if (!m_UsingEditorCamera) {
          m_UsingEditorCamera = true;
          m_LockedMousePosition = newMousePosition;
        }

        Input::SetCursorPosition(m_LockedMousePosition.x, m_LockedMousePosition.y);

        const glm::vec2 change = (newMousePosition - m_LockedMousePosition) * m_MouseSensitivity *
                                 Timestep::GetDeltaTime();
        finalYawPitch.x += change.x;
        finalYawPitch.y = glm::clamp(finalYawPitch.y - change.y, -89.9f, 89.9f);

        const float maxMoveSpeed = m_MovementSpeed * (ImGui::IsKeyDown(ImGuiKey_LeftShift) ? 3.0f : 1.0f) *
                                   Timestep::GetDeltaTime();
        if (ImGui::IsKeyDown(ImGuiKey_W))
          finalPosition += Camera.GetFront() * maxMoveSpeed;
        else if (ImGui::IsKeyDown(ImGuiKey_S))
          finalPosition -= Camera.GetFront() * maxMoveSpeed;
        if (ImGui::IsKeyDown(ImGuiKey_D))
          finalPosition += Camera.GetRight() * maxMoveSpeed;
        else if (ImGui::IsKeyDown(ImGuiKey_A))
          finalPosition -= Camera.GetRight() * maxMoveSpeed;

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

      const glm::vec3 dampedPosition = Math::SmoothDamp(position,
        finalPosition,
        m_TranslationVelocity,
        m_TranslationDampening,
        10000.0f,
        Timestep::GetDeltaTime());
      const glm::vec2 dampedYawPitch = Math::SmoothDamp(yawPitch,
        finalYawPitch,
        m_RotationVelocity,
        m_RotationDampening,
        1000.0f,
        Timestep::GetDeltaTime());

      Camera.SetPosition(m_SmoothCamera ? dampedPosition : finalPosition);
      Camera.SetYaw(m_SmoothCamera ? dampedYawPitch.x : finalYawPitch.x);
      Camera.SetPitch(m_SmoothCamera ? dampedYawPitch.y : finalYawPitch.y);

      Camera.Update();
    }
  }

  void ViewportPanel::DrawPerformanceOverlay() {
    if (!PerformanceOverlayVisible)
      return;
    static int corner = 1;
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                                    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                    ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    if (corner != -1) {
      constexpr float PAD_X = 20;
      constexpr float PAD_Y = 40;
      const ImGuiViewport* viewport = ImGui::GetMainViewport();
      const ImVec2 work_pos = m_ViewportOffset;
      const ImVec2 work_size = m_ViewportPanelSize;
      ImVec2 window_pos, window_pos_pivot;
      window_pos.x = corner & 1 ? work_pos.x + work_size.x - PAD_X : work_pos.x + PAD_X;
      window_pos.y = corner & 2 ? work_pos.y + work_size.y - PAD_Y : work_pos.y + PAD_Y;
      window_pos_pivot.x = corner & 1 ? 1.0f : 0.0f;
      window_pos_pivot.y = corner & 2 ? 1.0f : 0.0f;
      ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
      ImGui::SetNextWindowViewport(viewport->ID);
      window_flags |= ImGuiWindowFlags_NoMove;
    }
    ImGui::SetNextWindowBgAlpha(0.35f);
    if (ImGui::Begin("Performance Overlay", nullptr, window_flags)) {
      ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    }
    if (ImGui::BeginPopupContextWindow()) {
      if (ImGui::MenuItem("Custom", nullptr, corner == -1))
        corner = -1;
      if (ImGui::MenuItem("Top-left", nullptr, corner == 0))
        corner = 0;
      if (ImGui::MenuItem("Top-right", nullptr, corner == 1))
        corner = 1;
      if (ImGui::MenuItem("Bottom-left", nullptr, corner == 2))
        corner = 2;
      if (ImGui::MenuItem("Bottom-right", nullptr, corner == 3))
        corner = 3;
      if (PerformanceOverlayVisible && ImGui::MenuItem("Close"))
        PerformanceOverlayVisible = false;
      ImGui::EndPopup();
    }
    ImGui::End();
  }

  void ViewportPanel::DrawGizmos() {
    const Entity selectedEntity = m_SceneHierarchyPanel->GetSelectedEntity();
    if (selectedEntity && m_GizmoType != -1) {
      ImGuizmo::SetOrthographic(false);
      ImGuizmo::SetDrawlist();
      ImGuizmo::SetRect(m_ViewportBounds[0].x,
        m_ViewportBounds[0].y,
        m_ViewportBounds[1].x - m_ViewportBounds[0].x,
        m_ViewportBounds[1].y - m_ViewportBounds[0].y);

      const glm::mat4& cameraProjection = Camera.GetProjectionMatrix();
      const glm::mat4& cameraView = Camera.GetViewMatrix();

      auto& tc = selectedEntity.GetComponent<TransformComponent>();
      glm::mat4 transform = selectedEntity.GetWorldTransform();

      // Snapping
      const bool snap = Input::GetKey(Key::LeftControl);
      float snapValue = 0.5f; // Snap to 0.5m for translation/scale
      // Snap to 45 degrees for rotation
      if (m_GizmoType == ImGuizmo::OPERATION::ROTATE)
        snapValue = 45.0f;

      const float snapValues[3] = {snapValue, snapValue, snapValue};

      ImGuizmo::Manipulate(glm::value_ptr(cameraView),
        glm::value_ptr(cameraProjection),
        static_cast<ImGuizmo::OPERATION>(m_GizmoType),
        ImGuizmo::WORLD,
        glm::value_ptr(transform),
        nullptr,
        snap ? snapValues : nullptr);

      if (ImGuizmo::IsUsing()) {
        const Entity parent = selectedEntity.GetParent();
        const glm::mat4& parentWorldTransform = parent ? parent.GetWorldTransform() : glm::mat4(1.0f);
        glm::vec3 translation, rotation, scale;
        if (Math::DecomposeTransform(glm::inverse(parentWorldTransform) * transform, translation, rotation, scale)) {
          tc.Translation = translation;
          const glm::vec3 deltaRotation = rotation - tc.Rotation;
          tc.Rotation += deltaRotation;
          tc.Scale = scale;
        }
      }
    }
    if (Input::GetKey(Key::LeftControl)) {
      if (Input::GetKeyDown(Key::Q)) {
        if (!ImGuizmo::IsUsing())
          m_GizmoType = -1;
      }
      if (Input::GetKeyDown(Key::W)) {
        if (!ImGuizmo::IsUsing())
          m_GizmoType = ImGuizmo::OPERATION::TRANSLATE;
      }
      if (Input::GetKeyDown(Key::E)) {
        if (!ImGuizmo::IsUsing())
          m_GizmoType = ImGuizmo::OPERATION::ROTATE;
      }
      if (Input::GetKeyDown(Key::R)) {
        if (!ImGuizmo::IsUsing())
          m_GizmoType = ImGuizmo::OPERATION::SCALE;
      }
    }
  }
}
