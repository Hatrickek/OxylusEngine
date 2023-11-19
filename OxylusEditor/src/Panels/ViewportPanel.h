#pragma once

#include "EditorPanel.h"
#include "Render/Camera.h"
#include "Scene/Scene.h"
#include "SceneHierarchyPanel.h"

namespace Oxylus {
class ViewportPanel : public EditorPanel {
public:
  Camera m_camera;
  bool performance_overlay_visible = true;
  bool FullscreenViewport = false;

  ViewportPanel();

  void on_imgui_render() override;

  void set_context(const Ref<Scene>& scene, SceneHierarchyPanel& sceneHierarchyPanel);

  void on_update() override;

private:
  void draw_performance_overlay();
  void draw_gizmos();

  Ref<Scene> m_scene;
  SceneHierarchyPanel* scene_hierarchy_panel = nullptr;

  ImVec2 m_ViewportSize;
  ImVec2 m_ViewportBounds[2];
  ImVec2 viewport_panel_size = ImVec2();
  ImVec2 viewport_offset;
  ImVec2 m_GizmoPosition = ImVec2(1.0f, 1.0f);
  bool m_ViewportFocused{};
  bool m_ViewportHovered{};
  bool m_SimulationRunning = false;
  int m_GizmoType = -1;
  int m_GizmoMode = 0;

  //Camera
  bool m_LockAspectRatio = true;
  float m_TranslationDampening = 0.6f;
  float m_RotationDampening = 0.3f;
  bool m_SmoothCamera = true;
  float m_MouseSensitivity = 0.1f;
  float m_MovementSpeed = 5.0f;
  bool m_UseEditorCamera = true;
  bool m_UsingEditorCamera = false;
  glm::vec2 m_LockedMousePosition = glm::vec2(0.0f);
  Vec3 m_TranslationVelocity = Vec3(0);
  glm::vec2 m_RotationVelocity = glm::vec2(0);
};
}
