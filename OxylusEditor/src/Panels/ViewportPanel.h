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
  bool fullscreen_viewport = false;

  ViewportPanel();

  void on_imgui_render() override;

  void set_context(const Ref<Scene>& scene, SceneHierarchyPanel& scene_hierarchy_panel);

  void on_update() override;

private:
  void draw_performance_overlay();
  void draw_gizmos();
  void mouse_picking_pass(const Ref<RenderPipeline>& rp, const vuk::Dimension3D& dim, float fixed_width);
  bool outline_pass(const Ref<RenderPipeline>& rp, const vuk::Dimension3D& dim) const;

  Ref<Scene> m_scene = {};
  Entity m_hovered_entity = {};
  SceneHierarchyPanel* m_scene_hierarchy_panel = nullptr;

  Vec2 m_viewport_size = {};
  Vec2 m_viewport_bounds[2] = {};
  Vec2 viewport_panel_size = {};
  Vec2 viewport_offset = {};
  Vec2 m_gizmo_position = Vec2(1.0f, 1.0f);
  bool m_viewport_focused = {};
  bool m_viewport_hovered = {};
  bool m_simulation_running = false;
  int m_gizmo_type = -1;
  int m_gizmo_mode = 0;
  bool m_using_gizmo = false;

  std::vector<vuk::Unique<vuk::Buffer>> id_buffers = {};

  //Camera
  bool m_lock_aspect_ratio = true;
  float m_translation_dampening = 0.6f;
  float m_rotation_dampening = 0.3f;
  bool m_smooth_camera = true;
  float m_mouse_sensitivity = 0.1f;
  float m_movement_speed = 5.0f;
  bool m_use_editor_camera = true;
  bool m_using_editor_camera = false;
  Vec2 m_locked_mouse_position = Vec2(0.0f);
  Vec3 m_translation_velocity = Vec3(0);
  Vec2 m_rotation_velocity = Vec2(0);
};
}
