#include "src/oxpch.h"
#include "IGUI.h"

#include <format>
#include <imgui_internal.h>

#include "Assets/AssetManager.h"

#include "Utils/ImGuiScoped.h"
#include "Utils/UIUtils.h"

namespace Oxylus {
  inline static int s_UIContextID = 0;
  inline static int s_Counter = 0;
  char IGUI::s_IDBuffer[16];

  void IGUI::BeginProperties(ImGuiTableFlags flags) {
    s_IDBuffer[0] = '#';
    s_IDBuffer[1] = '#';
    memset(s_IDBuffer + 2, 0, 14);
    ++s_Counter;
    const std::string buffer = std::format("##{}", s_Counter);
    std::memcpy(&s_IDBuffer, buffer.data(), 16);

    constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_PadOuterX;
    ImGui::BeginTable(s_IDBuffer, 2, tableFlags | flags);
    ImGui::TableSetupColumn("PropertyName", 0, 0.5f);
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthStretch);
  }

  void IGUI::EndProperties() {
    ImGui::EndTable();
  }

  bool IGUI::Property(const char* label, bool& flag, const char* tooltip) {
    BeginPropertyGrid(label, tooltip);
    const bool modified = ImGui::Checkbox(s_IDBuffer, &flag);
    EndPropertyGrid();
    return modified;
  }

  bool IGUI::Property(const char* label, int& value, const char** dropdownStrings, int count, const char* tooltip) {
    BeginPropertyGrid(label, tooltip);

    bool modified = false;
    const char* current = dropdownStrings[value];

    if (ImGui::BeginCombo(s_IDBuffer, current)) {
      for (int i = 0; i < count; i++) {
        const bool isSelected = current == dropdownStrings[i];
        if (ImGui::Selectable(dropdownStrings[i], isSelected)) {
          current = dropdownStrings[i];
          value = i;
          modified = true;
        }

        if (isSelected)
          ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }

    IGUI::EndPropertyGrid();

    return modified;
  }

  bool IGUI::Property(const char* label, Ref <VulkanImage>& texture, uint64_t overrideTextureID, const char* tooltip) {
    BeginPropertyGrid(label, tooltip);

    bool changed = false;

    const float frameHeight = ImGui::GetFrameHeight();
    const float buttonSize = frameHeight * 3.0f;
    const ImVec2 xButtonSize = {buttonSize / 4.0f, buttonSize};
    const float tooltipSize = frameHeight * 11.0f;

    ImGui::SetCursorPos({
                                ImGui::GetContentRegionMax().x - buttonSize - xButtonSize.x,
                                ImGui::GetCursorPosY() + ImGui::GetStyle().FramePadding.y
                        });
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0, 0});
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.25f, 0.25f, 0.25f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.35f, 0.35f, 0.35f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.25f, 0.25f, 0.25f, 1.0f});

    ImTextureID id = {};
    if (id == nullptr)
      id = texture == nullptr ? 0 : texture->GetDescriptorSet();
    if (ImGui::ImageButton(id, {buttonSize, buttonSize}, {1, 1}, {0, 0}, 0)) {
      const auto& path = FileDialogs::OpenFile({{"Texture file", "png,ktx"}});
      if (!path.empty()) {
        const auto& asset = AssetManager::GetImageAsset(path);
        texture = asset.Data;
        changed = true;
      }
    }
    if (texture && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay)) {
      ImGui::BeginTooltip();
      ImGui::TextUnformatted(texture->GetDesc().Path.c_str());
      ImGui::Spacing();
      ImGui::Image(id, {tooltipSize, tooltipSize}, {1, 1}, {0, 0});
      ImGui::EndTooltip();
    }
    if (ImGui::BeginDragDropTarget()) {
      if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
        const auto path = GetPathFromImGuiPayload(payload);
        const auto& asset = AssetManager::GetImageAsset(AssetManager::GetAssetFileSystemPath(path).string());
        texture = asset.Data;
        changed = true;
      }
      ImGui::EndDragDropTarget();
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.2f, 0.2f, 0.2f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.3f, 0.3f, 0.3f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.2f, 0.2f, 0.2f, 1.0f});
    if (ImGui::Button("x", xButtonSize)) {
      texture = VulkanImage::GetBlankImage();
      changed = true;
    }
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    EndPropertyGrid();

    return changed;
  }

  void IGUI::DrawVec3Control(const char* label, glm::vec3& values, const char* tooltip, float resetValue) {
    BeginPropertyGrid(label, tooltip, false);

    const ImGuiIO& io = ImGui::GetIO();
    const auto boldFont = io.Fonts->Fonts[1];

    ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());

    const float frameHeight = ImGui::GetFrameHeight();
    const ImVec2 buttonSize = {frameHeight + 3.0f, frameHeight};

    const ImVec2 innerItemSpacing = ImGui::GetStyle().ItemInnerSpacing;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, innerItemSpacing);

    // X
    {
      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0, 0});
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1.0f, 1.0f, 1.0f, 1.0f});
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.8f, 0.1f, 0.15f, 1.0f});
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.9f, 0.2f, 0.2f, 1.0f});
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.8f, 0.1f, 0.15f, 1.0f});
      ImGui::PushFont(boldFont);
      if (ImGui::Button("X", buttonSize))
        values.x = resetValue;
      ImGui::PopFont();
      ImGui::PopStyleColor(4);

      ImGui::SameLine();
      ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");
      ImGui::PopItemWidth();
      ImGui::PopStyleVar();
    }

    ImGui::SameLine();

    // Y
    {
      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0, 0});
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1.0f, 1.0f, 1.0f, 1.0f});
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.2f, 0.7f, 0.2f, 1.0f});
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.3f, 0.8f, 0.3f, 1.0f});
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.2f, 0.7f, 0.2f, 1.0f});
      ImGui::PushFont(boldFont);
      if (ImGui::Button("Y", buttonSize))
        values.y = resetValue;
      ImGui::PopFont();
      ImGui::PopStyleColor(4);

      ImGui::SameLine();
      ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");
      ImGui::PopItemWidth();
      ImGui::PopStyleVar();
    }

    ImGui::SameLine();

    // Z
    {
      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0, 0});
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1.0f, 1.0f, 1.0f, 1.0f});
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.1f, 0.25f, 0.8f, 1.0f});
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.2f, 0.35f, 0.9f, 1.0f});
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.1f, 0.25f, 0.8f, 1.0f});
      ImGui::PushFont(boldFont);
      if (ImGui::Button("Z", buttonSize))
        values.z = resetValue;
      ImGui::PopFont();
      ImGui::PopStyleColor(4);

      ImGui::SameLine();
      ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f");
      ImGui::PopItemWidth();
      ImGui::PopStyleVar();
    }

    ImGui::PopStyleVar();

    EndPropertyGrid();
  }

  bool IGUI::ToggleButton(const char* label,
                          bool state,
                          ImVec4 defaultColor,
                          ImVec2 size,
                          float pressedAlpha,
                          ImGuiButtonFlags buttonFlags) {
    if (state) {
      ImVec4 color = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];

      color.w = pressedAlpha;
      ImGui::PushStyleColor(ImGuiCol_Button, color);
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
    }
    else {
      ImVec4 color = defaultColor;
      ImVec4 hoveredColor = ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered];
      hoveredColor.w = pressedAlpha;
      ImGui::PushStyleColor(ImGuiCol_Button, color);
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoveredColor);
      color.w = pressedAlpha;
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
    }

    const bool clicked = ImGui::ButtonEx(label, size, buttonFlags);

    ImGui::PopStyleColor(3);

    return clicked;
  }

  ImVec2 IGUI::GetIconButtonSize(const char8_t* icon, const char* label) {
    const float lineHeight = ImGui::GetTextLineHeight();
    const ImVec2 padding = ImGui::GetStyle().FramePadding;

    float width = ImGui::CalcTextSize(StringUtils::FromChar8T(icon)).x;
    width += ImGui::CalcTextSize(label).x;
    width += padding.x * 2.0f;

    return {width, lineHeight + padding.y * 2.0f};
  }

  bool IGUI::IconButton(const char8_t* icon, const char* label, ImVec4 iconColor) {
    ImGui::PushID(label);

    const float lineHeight = ImGui::GetTextLineHeight();
    const ImVec2 padding = ImGui::GetStyle().FramePadding;

    float width = ImGui::CalcTextSize(StringUtils::FromChar8T(icon)).x;
    width += ImGui::CalcTextSize(label).x;
    width += padding.x * 2.0f;
    float height = lineHeight + padding.y * 2.0f;

    const float cursorPosX = ImGui::GetCursorPosX();
    const bool clicked = ImGui::Button(label, {width, height});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0, 0});
    ImGui::SameLine();
    ImGui::SetCursorPosX(cursorPosX);
    ImGui::TextColored(iconColor, "%s", StringUtils::FromChar8T(icon));
    ImGui::SameLine();
    ImGui::TextUnformatted(label);
    ImGui::PopStyleVar();
    ImGui::PopID();

    return clicked;
  }

  std::filesystem::path IGUI::GetPathFromImGuiPayload(const ImGuiPayload* payload) {
    return static_cast<const wchar_t*>(payload->Data);
  }

  void IGUI::PushID() {
    ++s_UIContextID;
    ImGui::PushID(s_UIContextID);
    s_Counter = 0;
  }

  void IGUI::PopID() {
    ImGui::PopID();
    --s_UIContextID;
  }

  void IGUI::BeginPropertyGrid(const char* label, const char* tooltip, bool rightAlignNextColumn) {
    PushID();

    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    ImGui::PushID(label);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().FramePadding.y * 0.5f);
    ImGui::TextUnformatted(label);
    if (tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay)) {
      ImGui::BeginTooltip();
      ImGui::TextUnformatted(tooltip);
      ImGui::EndTooltip();
    }

    ImGui::TableNextColumn();

    if (rightAlignNextColumn)
      ImGui::SetNextItemWidth(-FLT_MIN);

    s_IDBuffer[0] = '#';
    s_IDBuffer[1] = '#';
    memset(s_IDBuffer + 2, 0, 14);
    ++s_Counter;
    const std::string buffer = std::format("##{}", s_Counter);
    std::memcpy(&s_IDBuffer, buffer.data(), 16);
  }

  void IGUI::EndPropertyGrid() {
    ImGui::PopID();
    PopID();
  }
}