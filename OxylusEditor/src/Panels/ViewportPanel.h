#pragma once

#include "EditorPanel.h"
#include "Render/Camera.h"
#include "Scene/Scene.h"
#include "SceneHierarchyPanel.h"

namespace Oxylus {
  class ViewportPanel : public EditorPanel {
  public:
    Camera Camera;
    bool PerformanceOverlayVisible = true;
    bool FullscreenViewport = false;

    ViewportPanel();

    void OnImGuiRender() override;

    void SetContext(const Ref<Scene>& scene, SceneHierarchyPanel& sceneHierarchyPanel);

    void OnUpdate() override;

  private:
    void DrawPerformanceOverlay();
    void DrawGizmos();

    Ref<Scene> m_Scene;
    SceneHierarchyPanel* m_SceneHierarchyPanel = nullptr;

    ImVec2 m_ViewportSize;
    ImVec2 m_ViewportBounds[2];
    ImVec2 m_ViewportPanelSize = ImVec2();
    ImVec2 m_ViewportOffset;
    bool m_ViewportFocused{};
    bool m_ViewportHovered{};
    bool m_SimulationRunning = false;
    int m_GizmoType = -1;

    //Camera
    bool m_LockAspectRatio = true;
    float m_TranslationDampening = 0.6f;
    float m_RotationDampening = 0.3f;
    bool m_SmoothCamera = true;
    float m_MouseSensitivity = 10.0f;
    float m_MovementSpeed = 120.0f;
    bool m_UseEditorCamera = true;
    bool m_UsingEditorCamera = false;
    glm::vec2 m_LockedMousePosition = glm::vec2(0.0f);
    Vec3 m_TranslationVelocity = Vec3(0);
    glm::vec2 m_RotationVelocity = glm::vec2(0);
  };
}
