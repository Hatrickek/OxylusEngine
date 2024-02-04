#include "SceneHierarchyPanel.h"

#include <Assets/AssetManager.h>
#include <icons/IconsMaterialDesignIcons.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <misc/cpp/imgui_stdlib.h>

#include "EditorLayer.h"

#include "Render/RendererCommon.h"

#include "Scene/EntitySerializer.h"
#include "UI/OxUI.h"
#include "Utils/ImGuiScoped.h"
#include "Utils/StringUtils.h"

namespace Ox {
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

void SceneHierarchyPanel::set_selected_entity(Entity entity) {
  m_selected_entity_entities.clear();
  m_selected_entity_entities.emplace_back(entity);
}

void SceneHierarchyPanel::set_context(const Shared<Scene>& scene) {
  m_context = scene;
  clear_selection_context();
}

ImRect SceneHierarchyPanel::draw_entity_node(Entity entity, uint32_t depth, bool force_expand_tree, bool is_part_of_prefab) {
  ImGui::TableNextRow();
  ImGui::TableNextColumn();

  const auto& rc = entity.get_relationship();
  const size_t children_size = rc.children.size();

  auto& tag_component = entity.get_component<TagComponent>();
  auto& tag = tag_component.tag;

  if (m_filter.IsActive() && !m_filter.PassFilter(tag.c_str())) {
    for (const auto& child_id : rc.children) {
      draw_entity_node(m_context->get_entity_by_uuid(child_id));
    }
    return {0, 0, 0, 0};
  }

  const auto is_selected = std::find(m_selected_entity_entities.begin(), m_selected_entity_entities.end(), entity) != m_selected_entity_entities.end();

  ImGuiTreeNodeFlags flags = (is_selected ? ImGuiTreeNodeFlags_Selected : 0);
  flags |= ImGuiTreeNodeFlags_OpenOnArrow;
  flags |= ImGuiTreeNodeFlags_SpanFullWidth;
  flags |= ImGuiTreeNodeFlags_FramePadding;

  if (children_size == 0) {
    flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
  }

  const bool highlight = is_selected;
  const auto header_selected_color = ImGuiLayer::header_selected_color;
  const auto popup_item_spacing = ImGuiLayer::popup_item_spacing;
  if (highlight) {
    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(header_selected_color));
    ImGui::PushStyleColor(ImGuiCol_Header, header_selected_color);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, header_selected_color);
  }

  if (force_expand_tree)
    ImGui::SetNextItemOpen(true);

  if (!is_part_of_prefab)
    is_part_of_prefab = entity.has_component<PrefabComponent>();
  const bool prefab_color_applied = is_part_of_prefab && !is_selected;
  if (prefab_color_applied)
    ImGui::PushStyleColor(ImGuiCol_Text, header_selected_color);

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
    force_expand_tree = opened;

  bool entity_deleted = false;

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, popup_item_spacing);
  if (ImGui::BeginPopupContextItem()) {
    if (get_selected_entity_front() != entity)
      m_selected_entity_entities.emplace_back(entity);

    if (ImGui::MenuItem("Rename", "F2"))
      m_renaming_entity = entity;
    if (ImGui::MenuItem("Duplicate", "Ctrl+D"))
      m_context->duplicate_entity(entity);
    if (ImGui::MenuItem("Delete", "Del"))
      entity_deleted = true;

    ImGui::Separator();

    draw_context_menu();

    ImGui::EndPopup();
  }
  ImGui::PopStyleVar();

  ImVec2 vertical_line_start = ImGui::GetCursorScreenPos();
  vertical_line_start.x -= 0.5f;
  vertical_line_start.y -= ImGui::GetFrameHeight() * 0.5f;

  // Drag Drop
  {
    if (ImGui::BeginDragDropTarget()) {
      if (const ImGuiPayload* entity_payload = ImGui::AcceptDragDropPayload("Entity")) {
        m_dragged_entity = *static_cast<Entity*>(entity_payload->Data);
        m_dragged_entity_target = entity;
      }
      else if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
        std::filesystem::path path = std::filesystem::path((const char*)payload->Data);
        path = AssetManager::get_asset_file_system_path(path);
        if (path.extension() == ".oxprefab") {
          m_dragged_entity = EntitySerializer::deserialize_entity_as_prefab(path.string().c_str(), m_context.get());
          m_dragged_entity = entity;
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

  if (entity == m_renaming_entity) {
    static bool renaming = false;
    if (!renaming) {
      renaming = true;
      ImGui::SetKeyboardFocusHere();
    }

    ImGui::InputText("##Tag", &tag);

    if (ImGui::IsItemDeactivated()) {
      renaming = false;
      m_renaming_entity = {};
    }
  }

  ImGui::TableNextColumn();

  ImGui::PushStyleColor(ImGuiCol_Button, {0, 0, 0, 0});
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0, 0, 0, 0});
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0, 0, 0, 0});

  const float button_size_x = ImGui::GetContentRegionAvail().x;
  const float frame_height = ImGui::GetFrameHeight();
  ImGui::Button(is_part_of_prefab ? "Prefab" : "Entity", {button_size_x, frame_height});
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
      reinterpret_cast<const char*>(tag_component.enabled
                                      ? ICON_MDI_EYE_OUTLINE
                                      : ICON_MDI_EYE_OFF_OUTLINE));

    if (!ImGui::IsItemHovered())
      tag_component.handled = false;

    if (ImGui::IsItemHovered() && ((!tag_component.handled && ImGui::IsMouseDragging(0)) || ImGui::IsItemClicked())) {
      tag_component.handled = true;
      tag_component.enabled = !tag_component.enabled;
    }
  }

  ImGui::PopStyleColor(3);

  if (prefab_color_applied)
    ImGui::PopStyleColor();

  // Open
  const ImRect node_rect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
  {
    if (opened && !entity_deleted) {
      ImColor tree_line_color;
      depth %= 4;
      switch (depth) {
        case 0: tree_line_color = ImColor(254, 112, 246);
          break;
        case 1: tree_line_color = ImColor(142, 112, 254);
          break;
        case 2: tree_line_color = ImColor(112, 180, 254);
          break;
        case 3: tree_line_color = ImColor(48, 134, 198);
          break;
        default: tree_line_color = ImColor(255, 255, 255);
          break;
      }

      ImDrawList* draw_list = ImGui::GetWindowDrawList();

      ImVec2 vertical_line_end = vertical_line_start;
      constexpr float line_thickness = 1.5f;

      for (const auto& child_id : rc.children) {
        Entity child = m_context->get_entity_by_uuid(child_id);
        const float horizontal_tree_line_size = child.get_relationship().children.empty() ? 18.0f : 9.0f;
        // chosen arbitrarily
        const ImRect child_rect = draw_entity_node(child, depth + 1, force_expand_tree, is_part_of_prefab);

        const float midpoint = (child_rect.Min.y + child_rect.Max.y) / 2.0f;
        draw_list->AddLine(ImVec2(vertical_line_start.x, midpoint),
          ImVec2(vertical_line_start.x + horizontal_tree_line_size, midpoint),
          tree_line_color,
          line_thickness);
        vertical_line_end.y = midpoint;
      }

      draw_list->AddLine(vertical_line_start, vertical_line_end, tree_line_color, line_thickness);
    }

    if (opened && children_size > 0)
      ImGui::TreePop();
  }

  // PostProcess Actions
  if (entity_deleted)
    m_deleted_entity = entity;

  return node_rect;
}

void SceneHierarchyPanel::drag_drop_target() const {
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
      const std::filesystem::path path = OxUI::get_path_from_imgui_payload(payload);
      if (path.extension() == ".oxscene") {
        EditorLayer::get()->open_scene(path);
      }
      if (path.extension() == ".gltf" || path.extension() == ".glb") {
        const auto mesh = AssetManager::get_mesh_asset(path.string());
        m_context->load_mesh(mesh);
      }
      if (path.extension() == ".oxprefab") {
        EntitySerializer::deserialize_entity_as_prefab(path.string().c_str(), m_context.get());
      }
    }

    ImGui::EndDragDropTarget();
  }
}

void SceneHierarchyPanel::draw_context_menu() {
  const bool has_context = get_selected_entity_front();

  if (!has_context)
    m_selected_entity_entities = {};

  Entity to_select = {};
  ImGuiScoped::StyleVar styleVar1(ImGuiStyleVar_ItemInnerSpacing, {0, 5});
  ImGuiScoped::StyleVar styleVar2(ImGuiStyleVar_ItemSpacing, {1, 5});
  if (ImGui::BeginMenu("Create")) {
    if (ImGui::MenuItem("Empty Entity")) {
      to_select = m_context->create_entity("New Entity");
    }

    if (ImGui::BeginMenu("Primitives")) {
      if (ImGui::MenuItem("Cube")) {
        m_context->load_mesh(AssetManager::get_mesh_asset("Resources/Objects/cube.glb"));
      }
      if (ImGui::MenuItem("Plane")) {
        m_context->load_mesh(AssetManager::get_mesh_asset("Resources/Objects/plane.glb"));
      }
      if (ImGui::MenuItem("Sphere")) {
        m_context->load_mesh(AssetManager::get_mesh_asset("Resources/Objects/sphere.glb"));
      }
      ImGui::EndMenu();
    }

    if (ImGui::MenuItem("Camera")) {
      to_select = m_context->create_entity("Camera");
      to_select.add_component_internal<CameraComponent>();
    }

    if (ImGui::MenuItem("Lua Script")) {
      to_select = m_context->create_entity("Script");
      to_select.add_component_internal<LuaScriptComponent>();
    }

    if (ImGui::BeginMenu("Light")) {
      if (ImGui::MenuItem("Sky Light")) {
        to_select = m_context->create_entity("Sky Light");
        to_select.add_component_internal<SkyLightComponent>();
      }
      if (ImGui::MenuItem("Light")) {
        to_select = m_context->create_entity("Light");
        to_select.add_component_internal<LightComponent>();
      }

      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Physics")) {
      using namespace JPH;

      if (ImGui::MenuItem("Sphere")) {
        to_select = m_context->create_entity("Sphere");
        to_select.add_component_internal<RigidbodyComponent>();
        to_select.add_component_internal<SphereColliderComponent>();
        to_select.add_component_internal<MeshComponent>(AssetManager::get_mesh_asset("Resources/Objects/sphere.glb"));
      }

      if (ImGui::MenuItem("Cube")) {
        to_select = m_context->create_entity("Cube");
        to_select.add_component_internal<RigidbodyComponent>();
        to_select.add_component_internal<BoxColliderComponent>();
        to_select.add_component_internal<MeshComponent>(AssetManager::get_mesh_asset("Resources/Objects/cube.glb"));
      }

      if (ImGui::MenuItem("Character Controller")) {
        to_select = m_context->create_entity("Character Controller");
        to_select.add_component_internal<CharacterControllerComponent>();
        to_select.add_component_internal<MeshComponent>(AssetManager::get_mesh_asset("Resources/Objects/capsule.glb"));
      }

      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Audio")) {
      if (ImGui::MenuItem("Audio Source")) {
        to_select = m_context->create_entity("AudioSource");
        to_select.add_component_internal<AudioSourceComponent>();
        ImGui::CloseCurrentPopup();
      }
      if (ImGui::MenuItem("Audio Listener")) {
        to_select = m_context->create_entity("AudioListener");
        to_select.add_component_internal<AudioListenerComponent>();
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Effects")) {
      if (ImGui::MenuItem("PostProcess Probe")) {
        to_select = m_context->create_entity("PostProcess Probe");
        to_select.add_component_internal<PostProcessProbe>();
      }
      if (ImGui::MenuItem("Particle System")) {
        to_select = m_context->create_entity("Particle System");
        to_select.add_component_internal<ParticleSystemComponent>();
      }
      ImGui::EndMenu();
    }

    ImGui::EndMenu();
  }

  if (has_context && to_select)
    to_select.set_parent(m_selected_entity_entities.front());

  if (to_select)
    m_selected_entity_entities.emplace_back(to_select);
}

SceneHierarchyPanel::SceneHierarchyPanel() : EditorPanel("Scene Hierarchy", ICON_MDI_VIEW_LIST, true) { }

void SceneHierarchyPanel::on_update() {
  //Handle shortcut inputs
  if (!m_selected_entity_entities.empty()) {
    if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_D)) {
      for (const auto& e : m_selected_entity_entities)
        m_context->duplicate_entity(e);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Delete) && m_table_hovered) {
      for (const auto& e : m_selected_entity_entities)
        m_context->destroy_entity(e);
      clear_selection_context();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F2)) {
      m_renaming_entity = get_selected_entity_front();
    }
  }
  if (m_deleted_entity) {
    if (get_selected_entity_front() == m_deleted_entity)
      clear_selection_context();

    m_context->destroy_entity(m_deleted_entity);
    m_deleted_entity = {};
  }
}

void SceneHierarchyPanel::on_imgui_render() {
  ImGuiScoped::StyleVar cellpad(ImGuiStyleVar_CellPadding, {0, 0});

  if (on_begin(ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar)) {
    const float line_height = ImGui::GetTextLineHeight();

    const ImVec2 padding = ImGui::GetStyle().FramePadding;
    constexpr ImGuiTableFlags table_flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_ContextMenuInBody |
                                           ImGuiTableFlags_BordersInner | ImGuiTableFlags_ScrollY;

    const float filter_cursor_pos_x = ImGui::GetCursorPosX();
    m_filter.Draw("###HierarchyFilter",
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

    if (!m_filter.IsActive()) {
      ImGui::SameLine();
      ImGui::SetCursorPosX(filter_cursor_pos_x + ImGui::GetFontSize() * 0.5f);
      ImGui::TextUnformatted(StringUtils::from_char8_t(ICON_MDI_MAGNIFY " Search..."));
    }

    const ImVec2 cursor_pos = ImGui::GetCursorPos();
    const ImVec2 region = ImGui::GetContentRegionAvail();
    if (region.x != 0.0f && region.y != 0.0f)
      ImGui::InvisibleButton("##DragDropTargetBehindTable", region);
    drag_drop_target();

    ImGui::SetCursorPos(cursor_pos);
    if (ImGui::BeginTable("HierarchyTable", 3, table_flags)) {
      ImGui::TableSetupColumn("  Label", ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_NoClip);
      ImGui::TableSetupColumn("  Type", ImGuiTableColumnFlags_WidthFixed, line_height * 3.0f);
      ImGui::TableSetupColumn(StringUtils::from_char8_t("  " ICON_MDI_EYE_OUTLINE),
        ImGuiTableColumnFlags_WidthFixed,
        line_height * 2.0f);

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
      const auto view = m_context->m_registry.view<IDComponent>();
      for (const auto e : view) {
        const Entity entity = {e, m_context.get()};
        if (entity && !entity.get_parent())
          draw_entity_node(entity);
      }
      ImGui::PopStyleVar();

      const auto pop_item_spacing = ImGuiLayer::popup_item_spacing;
      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, pop_item_spacing);
      if (ImGui::BeginPopupContextWindow("SceneHierarchyContextWindow",
        ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        clear_selection_context();
        draw_context_menu();
        ImGui::EndPopup();
      }
      ImGui::PopStyleVar();

      ImGui::EndTable();

      m_table_hovered = ImGui::IsItemHovered();

      if (ImGui::IsItemClicked())
        clear_selection_context();
    }
    m_window_hovered = ImGui::IsWindowHovered();

    if (ImGui::IsMouseDown(0) && m_window_hovered)
      clear_selection_context();

    if (m_dragged_entity && m_dragged_entity_target) {
      m_dragged_entity.set_parent(m_dragged_entity_target);
      m_dragged_entity = {};
      m_dragged_entity_target = {};
    }

    //OxUI::draw_gradient_shadow_bottom();

    on_end();
  }
}
}
