#pragma once

#include "EditorPanel.h"
#include "imgui_internal.h"
#include "Scene/Scene.h"
#include "Scene/Entity.h"

namespace Oxylus {
  class SceneHierarchyPanel : public EditorPanel {
  public:
    SceneHierarchyPanel();
    void on_update() override;
    void on_imgui_render() override;

    ImRect draw_entity_node(Entity entity, uint32_t depth = 0, bool force_expand_tree = false, bool is_part_of_prefab = false);
    void set_context(const Shared<Scene>& scene);

    void clear_selection_context();
    Entity get_selected_entity() const;
    void set_selected_entity(Entity entity);
    Shared<Scene> get_scene() const { return m_context; }

    void drag_drop_target() const;

  private:
    Shared<Scene> m_context = nullptr;
    ImGuiTextFilter m_filter;
    bool m_table_hovered = false;
    bool m_window_hovered = false;
    std::vector<Entity> m_selected_entity_entities;
    Entity m_renaming_entity;
    Entity m_dragged_entity;
    Entity m_dragged_entity_target;
    Entity m_deleted_entity;

    void draw_context_menu();
    Entity get_selected_entity_front() const;
  };
}
