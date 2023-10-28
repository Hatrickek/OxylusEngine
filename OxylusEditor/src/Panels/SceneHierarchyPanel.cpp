#include "SceneHierarchyPanel.h"

#include <Assets/AssetManager.h>
#include <icons/IconsMaterialDesignIcons.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <misc/cpp/imgui_stdlib.h>

#include "EditorLayer.h"
#include "Scene/EntitySerializer.h"
#include "UI/OxUI.h"
#include "Utils/ImGuiScoped.h"
#include "Utils/StringUtils.h"

namespace Oxylus {
void SceneHierarchyPanel::ClearSelectionContext() {
  m_SelectedEntity = {};
}

Entity SceneHierarchyPanel::GetSelectedEntity() const {
  return m_SelectedEntity;
}

void SceneHierarchyPanel::SetSelectedEntity(Entity entity) {
  m_SelectedEntity = entity;
}

void SceneHierarchyPanel::SetContext(const Ref<Scene>& scene) {
  m_Context = scene;
  m_SelectedEntity = {};
}

ImRect SceneHierarchyPanel::DrawEntityNode(Entity entity, uint32_t depth, bool forceExpandTree, bool isPartOfPrefab) {
  ImGui::TableNextRow();
  ImGui::TableNextColumn();

  const auto& rc = entity.get_relationship();
  const size_t childrenSize = rc.children.size();

  auto& tagComponent = entity.get_component<TagComponent>();
  auto& tag = tagComponent.tag;

  if (m_Filter.IsActive() && !m_Filter.PassFilter(tag.c_str())) {
    for (const auto& childId : rc.children) {
      DrawEntityNode(m_Context->get_entity_by_uuid(childId));
    }
    return {0, 0, 0, 0};
  }

  ImGuiTreeNodeFlags flags = (m_SelectedEntity == entity ? ImGuiTreeNodeFlags_Selected : 0);
  flags |= ImGuiTreeNodeFlags_OpenOnArrow;
  flags |= ImGuiTreeNodeFlags_SpanFullWidth;
  flags |= ImGuiTreeNodeFlags_FramePadding;

  if (childrenSize == 0) {
    flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
  }

  const bool highlight = m_SelectedEntity == entity;
  const auto headerSelectedColor = ImGuiLayer::HeaderSelectedColor;
  const auto popupItemSpacing = ImGuiLayer::PopupItemSpacing;
  if (highlight) {
    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(headerSelectedColor));
    ImGui::PushStyleColor(ImGuiCol_Header, headerSelectedColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, headerSelectedColor);
  }

  if (forceExpandTree)
    ImGui::SetNextItemOpen(true);

  if (!isPartOfPrefab)
    isPartOfPrefab = entity.has_component<PrefabComponent>();
  const bool prefabColorApplied = isPartOfPrefab && entity != m_SelectedEntity;
  if (prefabColorApplied)
    ImGui::PushStyleColor(ImGuiCol_Text, headerSelectedColor);

  const bool opened = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uint64_t>(entity.get_uuid())),
    flags,
    "%s %s",
    StringUtils::from_char8_t(ICON_MDI_CUBE_OUTLINE),
    tag.c_str());

  if (highlight)
    ImGui::PopStyleColor(2);

  // Select
  if (!ImGui::IsItemToggledOpen() && (ImGui::IsItemClicked(ImGuiMouseButton_Left) ||
                                      ImGui::IsItemClicked(ImGuiMouseButton_Middle) || ImGui::IsItemClicked(
                                        ImGuiMouseButton_Right))) {
    m_SelectedEntity = entity;
  }

  // Expand recursively
  if (ImGui::IsItemToggledOpen() && (ImGui::IsKeyDown(ImGuiKey_LeftAlt) || ImGui::IsKeyDown(ImGuiKey_RightAlt)))
    forceExpandTree = opened;

  bool entityDeleted = false;

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, popupItemSpacing);
  if (ImGui::BeginPopupContextItem()) {
    if (m_SelectedEntity != entity)
      m_SelectedEntity = entity;

    if (ImGui::MenuItem("Rename", "F2"))
      m_RenamingEntity = entity;
    if (ImGui::MenuItem("Duplicate", "Ctrl+D"))
      m_Context->duplicate_entity(entity);
    if (ImGui::MenuItem("Delete", "Del"))
      entityDeleted = true;

    ImGui::Separator();

    DrawContextMenu();

    ImGui::EndPopup();
  }
  ImGui::PopStyleVar();

  ImVec2 verticalLineStart = ImGui::GetCursorScreenPos();
  verticalLineStart.x -= 0.5f;
  verticalLineStart.y -= ImGui::GetFrameHeight() * 0.5f;

  // Drag Drop
  {
    if (ImGui::BeginDragDropTarget()) {
      if (const ImGuiPayload* entityPayload = ImGui::AcceptDragDropPayload("Entity")) {
        m_DraggedEntity = *static_cast<Entity*>(entityPayload->Data);
        m_DraggedEntityTarget = entity;
      }
      else if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
        std::filesystem::path path = std::filesystem::path((const char*)payload->Data);
        path = AssetManager::get_asset_file_system_path(path);
        if (path.extension() == ".oxprefab") {
          m_DraggedEntity = EntitySerializer::DeserializeEntityAsPrefab(path.string().c_str(), m_Context.get());
          m_DraggedEntity = entity;
        }
      }

      ImGui::EndDragDropTarget();
    }

    if (ImGui::BeginDragDropSource()) {
      ImGui::SetDragDropPayload("Entity", &entity, sizeof(entity));
      ImGui::TextUnformatted(tag.c_str());
      ImGui::EndDragDropSource();
    }
  }

  if (entity == m_RenamingEntity) {
    static bool renaming = false;
    if (!renaming) {
      renaming = true;
      ImGui::SetKeyboardFocusHere();
    }

    ImGui::InputText("##Tag", &tag);

    if (ImGui::IsItemDeactivated()) {
      renaming = false;
      m_RenamingEntity = {};
    }
  }

  ImGui::TableNextColumn();

  ImGui::PushStyleColor(ImGuiCol_Button, {0, 0, 0, 0});
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0, 0, 0, 0});
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0, 0, 0, 0});

  const float buttonSizeX = ImGui::GetContentRegionAvail().x;
  const float frameHeight = ImGui::GetFrameHeight();
  ImGui::Button(isPartOfPrefab ? "Prefab" : "Entity", {buttonSizeX, frameHeight});
  // Select
  if (ImGui::IsItemDeactivated() && ImGui::IsItemHovered() && !ImGui::IsItemToggledOpen())
    m_SelectedEntity = entity;

  ImGui::TableNextColumn();
  // Visibility Toggle
  {
    ImGui::Text("  %s",
      reinterpret_cast<const char*>(tagComponent.enabled
                                      ? ICON_MDI_EYE_OUTLINE
                                      : ICON_MDI_EYE_OFF_OUTLINE));

    if (!ImGui::IsItemHovered())
      tagComponent.handled = false;

    if (ImGui::IsItemHovered() && ((!tagComponent.handled && ImGui::IsMouseDragging(0)) || ImGui::IsItemClicked())) {
      tagComponent.handled = true;
      tagComponent.enabled = !tagComponent.enabled;
    }
  }

  ImGui::PopStyleColor(3);

  if (prefabColorApplied)
    ImGui::PopStyleColor();

  // Open
  const ImRect nodeRect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
  {
    if (opened && !entityDeleted) {
      ImColor treeLineColor;
      depth %= 4;
      switch (depth) {
        case 0: treeLineColor = ImColor(254, 112, 246);
          break;
        case 1: treeLineColor = ImColor(142, 112, 254);
          break;
        case 2: treeLineColor = ImColor(112, 180, 254);
          break;
        case 3: treeLineColor = ImColor(48, 134, 198);
          break;
        default: treeLineColor = ImColor(255, 255, 255);
          break;
      }

      ImDrawList* drawList = ImGui::GetWindowDrawList();

      ImVec2 verticalLineEnd = verticalLineStart;
      constexpr float lineThickness = 1.5f;

      for (const auto& childId : rc.children) {
        Entity child = m_Context->get_entity_by_uuid(childId);
        const float HorizontalTreeLineSize = child.get_relationship().children.empty() ? 18.0f : 9.0f;
        // chosen arbitrarily
        const ImRect childRect = DrawEntityNode(child, depth + 1, forceExpandTree, isPartOfPrefab);

        const float midpoint = (childRect.Min.y + childRect.Max.y) / 2.0f;
        drawList->AddLine(ImVec2(verticalLineStart.x, midpoint),
          ImVec2(verticalLineStart.x + HorizontalTreeLineSize, midpoint),
          treeLineColor,
          lineThickness);
        verticalLineEnd.y = midpoint;
      }

      drawList->AddLine(verticalLineStart, verticalLineEnd, treeLineColor, lineThickness);
    }

    if (opened && childrenSize > 0)
      ImGui::TreePop();
  }

  // PostProcess Actions
  if (entityDeleted)
    m_DeletedEntity = entity;

  return nodeRect;
}

void SceneHierarchyPanel::DragDropTarget() const {
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
      const std::filesystem::path path = OxUI::get_path_from_imgui_payload(payload);
      if (path.extension() == ".oxscene") {
        EditorLayer::get()->OpenScene(path);
      }
      if (path.extension() == ".gltf" || path.extension() == ".glb") {
        m_Context->create_entity_with_mesh(AssetManager::get_mesh_asset(path.string()));
      }
      if (path.extension() == ".oxprefab") {
        EntitySerializer::DeserializeEntityAsPrefab(path.string().c_str(), m_Context.get());
      }
    }

    ImGui::EndDragDropTarget();
  }
}

void SceneHierarchyPanel::DrawContextMenu() {
  const bool hasContext = m_SelectedEntity;

  if (!hasContext)
    m_SelectedEntity = {};

  Entity toSelect = {};
  ImGuiScoped::StyleVar styleVar1(ImGuiStyleVar_ItemInnerSpacing, {0, 5});
  ImGuiScoped::StyleVar styleVar2(ImGuiStyleVar_ItemSpacing, {1, 5});
  if (ImGui::BeginMenu("Create")) {
    if (ImGui::MenuItem("Empty Entity")) {
      toSelect = m_Context->create_entity("New Entity");
    }

    if (ImGui::BeginMenu("Primitives")) {
      if (ImGui::MenuItem("Cube")) {
        toSelect = m_Context->create_entity("Cube");
        toSelect.add_component_internal<MeshRendererComponent>(AssetManager::get_mesh_asset("Resources/Objects/cube.glb"));
      }
      if (ImGui::MenuItem("Plane")) {
        toSelect = m_Context->create_entity("Plane");
        toSelect.add_component_internal<MeshRendererComponent>(AssetManager::get_mesh_asset("Resources/Objects/plane.gltf"));
      }
      if (ImGui::MenuItem("Sphere")) {
        toSelect = m_Context->create_entity("Sphere");
        toSelect.add_component_internal<MeshRendererComponent>(AssetManager::get_mesh_asset("Resources/Objects/sphere.gltf"));
      }
      ImGui::EndMenu();
    }

    if (ImGui::MenuItem("Camera")) {
      toSelect = m_Context->create_entity("Camera");
      toSelect.add_component_internal<CameraComponent>();
    }

    if (ImGui::BeginMenu("Light")) {
      if (ImGui::MenuItem("Sky Light")) {
        toSelect = m_Context->create_entity("Sky Light");
        toSelect.add_component_internal<SkyLightComponent>();
      }
      if (ImGui::MenuItem("Light")) {
        toSelect = m_Context->create_entity("Light");
        toSelect.add_component_internal<LightComponent>();
      }

      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Physics")) {
      using namespace JPH;

      if (ImGui::MenuItem("Sphere")) {
        toSelect = m_Context->create_entity("Sphere");
        toSelect.add_component_internal<RigidbodyComponent>();
        toSelect.add_component_internal<SphereColliderComponent>();
        toSelect.add_component_internal<MeshRendererComponent>(AssetManager::get_mesh_asset("Resources/Objects/sphere.gltf"));
      }

      if (ImGui::MenuItem("Cube")) {
        toSelect = m_Context->create_entity("Cube");
        toSelect.add_component_internal<RigidbodyComponent>();
        toSelect.add_component_internal<BoxColliderComponent>();
        toSelect.add_component_internal<MeshRendererComponent>(AssetManager::get_mesh_asset("Resources/Objects/cube.glb"));
      }

      if (ImGui::MenuItem("Character Controller")) {
        toSelect = m_Context->create_entity("Character Controller");
        toSelect.add_component_internal<CharacterControllerComponent>();
        toSelect.add_component_internal<MeshRendererComponent>(AssetManager::get_mesh_asset("Resources/Objects/capsule.glb"));
      }

      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Audio")) {
      if (ImGui::MenuItem("Audio Source")) {
        toSelect = m_Context->create_entity("AudioSource");
        toSelect.add_component_internal<AudioSourceComponent>();
        ImGui::CloseCurrentPopup();
      }
      if (ImGui::MenuItem("Audio Listener")) {
        toSelect = m_Context->create_entity("AudioListener");
        toSelect.add_component_internal<AudioListenerComponent>();
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Effects")) {
      if (ImGui::MenuItem("PostProcess Probe")) {
        toSelect = m_Context->create_entity("PostProcess Probe");
        toSelect.add_component_internal<PostProcessProbe>();
      }
      if (ImGui::MenuItem("Particle System")) {
        toSelect = m_Context->create_entity("Particle System");
        toSelect.add_component_internal<ParticleSystemComponent>();
      }
      ImGui::EndMenu();
    }

    ImGui::EndMenu();
  }

  if (hasContext && toSelect)
    toSelect.set_parent(m_SelectedEntity);

  if (toSelect)
    m_SelectedEntity = toSelect;
}

SceneHierarchyPanel::SceneHierarchyPanel() : EditorPanel("Scene Hierarchy", ICON_MDI_VIEW_LIST, true) { }

void SceneHierarchyPanel::on_update() {
  //Handle shortcut inputs
  if (m_SelectedEntity) {
    if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_D)) {
      m_Context->duplicate_entity(m_SelectedEntity);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Delete) && m_TableHovered) {
      m_Context->destroy_entity(m_SelectedEntity);
      m_SelectedEntity = {};
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F2)) {
      m_RenamingEntity = m_SelectedEntity;
    }
  }
  if (m_DeletedEntity) {
    if (m_SelectedEntity == m_DeletedEntity)
      ClearSelectionContext();

    m_Context->destroy_entity(m_DeletedEntity);
    m_DeletedEntity = {};
  }
}

void SceneHierarchyPanel::on_imgui_render() {
  ImGuiScoped::StyleVar cellpad(ImGuiStyleVar_CellPadding, {0, 0});

  if (on_begin(ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar)) {
    const float lineHeight = ImGui::GetTextLineHeight();

    const ImVec2 padding = ImGui::GetStyle().FramePadding;
    constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_ContextMenuInBody |
                                           ImGuiTableFlags_BordersInner | ImGuiTableFlags_ScrollY;

    const float filterCursorPosX = ImGui::GetCursorPosX();
    m_Filter.Draw("###HierarchyFilter",
      ImGui::GetContentRegionAvail().x -
      (OxUI::get_icon_button_size(ICON_MDI_PLUS, "").x + 2.0f * padding.x));
    ImGui::SameLine();

    if (ImGui::Button(StringUtils::from_char8_t(ICON_MDI_PLUS)))
      ImGui::OpenPopup("SceneHierarchyContextWindow");

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 8.0f));
    if (ImGui::BeginPopupContextWindow("SceneHierarchyContextWindow",
      ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
      DrawContextMenu();
      ImGui::EndPopup();
    }
    ImGui::PopStyleVar();

    if (!m_Filter.IsActive()) {
      ImGui::SameLine();
      ImGui::SetCursorPosX(filterCursorPosX + ImGui::GetFontSize() * 0.5f);
      ImGui::TextUnformatted(StringUtils::from_char8_t(ICON_MDI_MAGNIFY " Search..."));
    }

    const ImVec2 cursorPos = ImGui::GetCursorPos();
    const ImVec2 region = ImGui::GetContentRegionAvail();
    ImGui::InvisibleButton("##DragDropTargetBehindTable", region);
    DragDropTarget();

    ImGui::SetCursorPos(cursorPos);
    if (ImGui::BeginTable("HierarchyTable", 3, tableFlags)) {
      ImGui::TableSetupColumn("  Label", ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_NoClip);
      ImGui::TableSetupColumn("  Type", ImGuiTableColumnFlags_WidthFixed, lineHeight * 3.0f);
      ImGui::TableSetupColumn(StringUtils::from_char8_t("  " ICON_MDI_EYE_OUTLINE),
        ImGuiTableColumnFlags_WidthFixed,
        lineHeight * 2.0f);

      ImGui::TableSetupScrollFreeze(0, 1);

      ImGui::TableNextRow(ImGuiTableRowFlags_Headers, ImGui::GetFrameHeight());

      for (int column = 0; column < 3; ++column) {
        ImGui::TableSetColumnIndex(column);
        const char* column_name = ImGui::TableGetColumnName(column);
        ImGui::PushID(column);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + padding.y);
        ImGui::TableHeader(column_name);
        ImGui::PopID();
      }

      ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
      const auto view = m_Context->m_registry.view<IDComponent>();
      for (const auto e : view) {
        const Entity entity = {e, m_Context.get()};
        if (entity && !entity.get_parent())
          DrawEntityNode(entity);
      }
      ImGui::PopStyleVar();

      const auto popItemSpacing = ImGuiLayer::PopupItemSpacing;
      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, popItemSpacing);
      if (ImGui::BeginPopupContextWindow("SceneHierarchyContextWindow",
        ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        ClearSelectionContext();
        DrawContextMenu();
        ImGui::EndPopup();
      }
      ImGui::PopStyleVar();

      ImGui::EndTable();

      m_TableHovered = ImGui::IsItemHovered();

      if (ImGui::IsItemClicked())
        ClearSelectionContext();
    }
    m_WindowHovered = ImGui::IsWindowHovered();

    if (ImGui::IsMouseDown(0) && m_WindowHovered)
      ClearSelectionContext();

    if (m_DraggedEntity && m_DraggedEntityTarget) {
      m_DraggedEntity.set_parent(m_DraggedEntityTarget);
      m_DraggedEntity = {};
      m_DraggedEntityTarget = {};
    }

    OxUI::draw_gradient_shadow();

    on_end();
  }
}
}
