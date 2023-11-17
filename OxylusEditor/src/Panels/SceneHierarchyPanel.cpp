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
Entity SceneHierarchyPanel::get_selected_entity_front() const {
  if (m_selected_entity_entities.empty())
    return {};
  return m_selected_entity_entities.front();
}

void SceneHierarchyPanel::clear_selection_context() {
  m_selected_entity_entities.clear();
}

Entity SceneHierarchyPanel::get_selected_entity() const {
  return get_selected_entity_front();
}

void SceneHierarchyPanel::set_context(const Ref<Scene>& scene) {
  m_Context = scene;
  clear_selection_context();
}

ImRect SceneHierarchyPanel::draw_entity_node(Entity entity, uint32_t depth, bool forceExpandTree, bool isPartOfPrefab) {
  ImGui::TableNextRow();
  ImGui::TableNextColumn();

  const auto& rc = entity.get_relationship();
  const size_t childrenSize = rc.children.size();

  auto& tagComponent = entity.get_component<TagComponent>();
  auto& tag = tagComponent.tag;

  if (m_Filter.IsActive() && !m_Filter.PassFilter(tag.c_str())) {
    for (const auto& childId : rc.children) {
      draw_entity_node(m_Context->get_entity_by_uuid(childId));
    }
    return {0, 0, 0, 0};
  }

  const auto is_selected = std::find(m_selected_entity_entities.begin(), m_selected_entity_entities.end(), entity) != m_selected_entity_entities.end();

  ImGuiTreeNodeFlags flags = (is_selected ? ImGuiTreeNodeFlags_Selected : 0);
  flags |= ImGuiTreeNodeFlags_OpenOnArrow;
  flags |= ImGuiTreeNodeFlags_SpanFullWidth;
  flags |= ImGuiTreeNodeFlags_FramePadding;

  if (childrenSize == 0) {
    flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
  }

  const bool highlight = is_selected;
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
  const bool prefabColorApplied = isPartOfPrefab && !is_selected;
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
    if (!ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) 
      m_selected_entity_entities = {};
    m_selected_entity_entities.emplace_back(entity);
  }

  // Expand recursively
  if (ImGui::IsItemToggledOpen() && (ImGui::IsKeyDown(ImGuiKey_LeftAlt) || ImGui::IsKeyDown(ImGuiKey_RightAlt)))
    forceExpandTree = opened;

  bool entityDeleted = false;

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, popupItemSpacing);
  if (ImGui::BeginPopupContextItem()) {
    if (get_selected_entity_front() != entity)
      m_selected_entity_entities.emplace_back(entity);

    if (ImGui::MenuItem("Rename", "F2"))
      m_RenamingEntity = entity;
    if (ImGui::MenuItem("Duplicate", "Ctrl+D"))
      m_Context->duplicate_entity(entity);
    if (ImGui::MenuItem("Delete", "Del"))
      entityDeleted = true;

    ImGui::Separator();

    draw_context_menu();

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
  if (ImGui::IsItemDeactivated() && ImGui::IsItemHovered() && !ImGui::IsItemToggledOpen()) {
    if (!ImGui::IsKeyDown(ImGuiKey_LeftCtrl))
      clear_selection_context();
    m_selected_entity_entities.emplace_back(entity);
  }


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
        const ImRect childRect = draw_entity_node(child, depth + 1, forceExpandTree, isPartOfPrefab);

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

void SceneHierarchyPanel::drag_drop_target() const {
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
      const std::filesystem::path path = OxUI::get_path_from_imgui_payload(payload);
      if (path.extension() == ".oxscene") {
        EditorLayer::get()->open_scene(path);
      }
      if (path.extension() == ".gltf" || path.extension() == ".glb") {
        m_Context->load_mesh(AssetManager::get_mesh_asset(path.string()));
      }
      if (path.extension() == ".oxprefab") {
        EntitySerializer::DeserializeEntityAsPrefab(path.string().c_str(), m_Context.get());
      }
    }

    ImGui::EndDragDropTarget();
  }
}

void SceneHierarchyPanel::draw_context_menu() {
  const bool hasContext = get_selected_entity_front();

  if (!hasContext)
    m_selected_entity_entities = {};

  Entity toSelect = {};
  ImGuiScoped::StyleVar styleVar1(ImGuiStyleVar_ItemInnerSpacing, {0, 5});
  ImGuiScoped::StyleVar styleVar2(ImGuiStyleVar_ItemSpacing, {1, 5});
  if (ImGui::BeginMenu("Create")) {
    if (ImGui::MenuItem("Empty Entity")) {
      toSelect = m_Context->create_entity("New Entity");
    }

    if (ImGui::BeginMenu("Primitives")) {
      if (ImGui::MenuItem("Cube")) {
        m_Context->load_mesh(AssetManager::get_mesh_asset("Resources/Objects/cube.glb"));
      }
      if (ImGui::MenuItem("Plane")) {
        m_Context->load_mesh(AssetManager::get_mesh_asset("Resources/Objects/plane.glb"));
      }
      if (ImGui::MenuItem("Sphere")) {
        m_Context->load_mesh(AssetManager::get_mesh_asset("Resources/Objects/sphere.gltf"));
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
        toSelect.add_component_internal<MeshComponent>(AssetManager::get_mesh_asset("Resources/Objects/sphere.gltf"));
      }

      if (ImGui::MenuItem("Cube")) {
        toSelect = m_Context->create_entity("Cube");
        toSelect.add_component_internal<RigidbodyComponent>();
        toSelect.add_component_internal<BoxColliderComponent>();
        toSelect.add_component_internal<MeshComponent>(AssetManager::get_mesh_asset("Resources/Objects/cube.glb"));
      }

      if (ImGui::MenuItem("Character Controller")) {
        toSelect = m_Context->create_entity("Character Controller");
        toSelect.add_component_internal<CharacterControllerComponent>();
        toSelect.add_component_internal<MeshComponent>(AssetManager::get_mesh_asset("Resources/Objects/capsule.glb"));
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
    toSelect.set_parent(m_selected_entity_entities.front());

  if (toSelect)
    m_selected_entity_entities.emplace_back(toSelect);
}

SceneHierarchyPanel::SceneHierarchyPanel() : EditorPanel("Scene Hierarchy", ICON_MDI_VIEW_LIST, true) { }

void SceneHierarchyPanel::on_update() {
  //Handle shortcut inputs
  if (!m_selected_entity_entities.empty()) {
    if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_D)) {
      for (const auto& e : m_selected_entity_entities)
        m_Context->duplicate_entity(e);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Delete) && m_TableHovered) {
      for (const auto& e : m_selected_entity_entities)
        m_Context->destroy_entity(e);
      clear_selection_context();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F2)) {
      m_RenamingEntity = get_selected_entity_front();
    }
  }
  if (m_DeletedEntity) {
    if (get_selected_entity_front() == m_DeletedEntity)
      clear_selection_context();

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
      draw_context_menu();
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
    if (region.x != 0.0f && region.y != 0.0f)
      ImGui::InvisibleButton("##DragDropTargetBehindTable", region);
    drag_drop_target();

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
          draw_entity_node(entity);
      }
      ImGui::PopStyleVar();

      const auto popItemSpacing = ImGuiLayer::PopupItemSpacing;
      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, popItemSpacing);
      if (ImGui::BeginPopupContextWindow("SceneHierarchyContextWindow",
        ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        clear_selection_context();
        draw_context_menu();
        ImGui::EndPopup();
      }
      ImGui::PopStyleVar();

      ImGui::EndTable();

      m_TableHovered = ImGui::IsItemHovered();

      if (ImGui::IsItemClicked())
        clear_selection_context();
    }
    m_WindowHovered = ImGui::IsWindowHovered();

    if (ImGui::IsMouseDown(0) && m_WindowHovered)
      clear_selection_context();

    if (m_DraggedEntity && m_DraggedEntityTarget) {
      m_DraggedEntity.set_parent(m_DraggedEntityTarget);
      m_DraggedEntity = {};
      m_DraggedEntityTarget = {};
    }

    //OxUI::draw_gradient_shadow_bottom();

    on_end();
  }
}
}
