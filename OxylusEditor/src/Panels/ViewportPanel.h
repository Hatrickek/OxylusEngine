#pragma once

#include "EditorPanel.h"
#include "EditorTheme.h"

#include "Render/Camera.h"
#include "Scene/Scene.h"
#include "SceneHierarchyPanel.h"

#include "UI/ImGuiLayer.h"
#include "UI/OxUI.h"

#include "Utils/StringUtils.h"

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

  template <typename T>
  void show_component_gizmo(const float width,
                            const float height,
                            const float xpos,
                            const float ypos,
                            const Mat4& view_proj,
                            const Frustum& frustum,
                            Scene* scene) {
    if (m_show_gizmo_image_map[typeid(T).hash_code()]) {
      auto view = scene->m_registry.view<TransformComponent, T>();

      for (const auto&& [entity, transform, component] : view.each()) {
        Vec3 pos = Entity(entity, scene).get_world_transform()[3];

        const auto inside = frustum.is_inside(pos);

        if (inside == (uint32_t)Intersection::Outside)
          continue;

        const Vec2 screen_pos = Math::world_to_screen(pos, view_proj, width, height, xpos, ypos);
        ImGui::SetCursorPos({screen_pos.x - ImGui::GetFontSize() * 0.5f, screen_pos.y - ImGui::GetFontSize() * 0.5f});
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.7f, 0.7f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.7f, 0.7f, 0.0f));

        if (OxUI::image_button("##", m_show_gizmo_image_map[typeid(T).hash_code()]->get_texture(), {40.f, 40.f})) {
          m_scene_hierarchy_panel->set_selected_entity(Entity(entity, scene));
        }

        ImGui::PopStyleColor(2);

        std::string name_s = typeid(T).name();
        auto demangled_name = name_s.substr(name_s.find(' '));

        OxUI::tooltip(demangled_name.c_str());
      }
    }
  }

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

  std::unordered_map<size_t, Ref<TextureAsset>> m_show_gizmo_image_map;

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
