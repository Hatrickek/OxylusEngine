#pragma once

#include "EditorPanel.h"
#include "imgui_internal.h"
#include "Scene/Scene.h"
#include "Core/Entity.h"

namespace Oxylus {
  class SceneHierarchyPanel : public EditorPanel {
  public:
    SceneHierarchyPanel();
    void OnUpdate() override;
    void OnImGuiRender() override;

    ImRect DrawEntityNode(Entity entity, uint32_t depth = 0, bool forceExpandTree = false, bool isPartOfPrefab = false);
    void SetContext(const Ref<Scene>& scene);

    void ClearSelectionContext();
    Entity GetSelectedEntity() const;
    Ref<Scene> GetScene() const { return m_Context; }
    void SetSelectedEntity(Entity entity);

    void DragDropTarget() const;

  private:
    bool m_TableHovered = false;
    bool m_WindowHovered = false;
    Entity m_SelectedEntity;
    Entity m_RenamingEntity;
    Entity m_DraggedEntity;
    Entity m_DraggedEntityTarget;
    Entity m_DeletedEntity;
    void DrawContextMenu();

    Ref<Scene> m_Context = nullptr;

    ImGuiTextFilter m_Filter;
  };
}
