#include "OxUI.h"

#include "Assets/AssetManager.h"
#include "Utils/ImGuiScoped.h"
#include "Utils/StringUtils.h"
#include "Utils/UIUtils.h"

#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>
#include <fmt/format.h>
#include <vuk/SampledImage.hpp>

#include "Assets/TextureAsset.h"
#include "Core/Application.h"

namespace Oxylus {
inline static int ui_context_id = 0;
inline static int s_Counter = 0;
char OxUI::id_buffer[16];

bool OxUI::begin_properties(ImGuiTableFlags flags) {
  id_buffer[0] = '#';
  id_buffer[1] = '#';
  memset(id_buffer + 2, 0, 14);
  ++s_Counter;
  const std::string buffer = fmt::format("##{}", s_Counter);
  std::memcpy(&id_buffer, buffer.data(), 16);

  constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_PadOuterX;
  if (ImGui::BeginTable(id_buffer, 2, tableFlags | flags)) {
    ImGui::TableSetupColumn("PropertyName", 0, 0.5f);
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthStretch);
    return true;
  }
  return false;
}

void OxUI::end_properties() {
  ImGui::EndTable();
}

void OxUI::text(const char* text1, const char* text2, const char* tooltip) {
  begin_property_grid(text1, tooltip);
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().FramePadding.y * 0.5f);
  ImGui::Text("%s", text2);
  end_property_grid();
}

bool OxUI::property(const char* label, bool* flag, const char* tooltip) {
  begin_property_grid(label, tooltip);
  const bool modified = ImGui::Checkbox(id_buffer, flag);
  end_property_grid();
  return modified;
}

bool OxUI::property(const char* label, std::string* text, ImGuiInputFlags flags, const char* tooltip) {
  begin_property_grid(label, tooltip);
  const bool modified = ImGui::InputText(id_buffer, text, flags);
  end_property_grid();
  return modified;
}

bool OxUI::property(const char* label, int* value, const char** dropdownStrings, int count, const char* tooltip) {
  begin_property_grid(label, tooltip);

  bool modified = false;
  const char* current = dropdownStrings[*value];

  if (ImGui::BeginCombo(id_buffer, current)) {
    for (int i = 0; i < count; i++) {
      const bool isSelected = current == dropdownStrings[i];
      if (ImGui::Selectable(dropdownStrings[i], isSelected)) {
        current = dropdownStrings[i];
        *value = i;
        modified = true;
      }

      if (isSelected)
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }

  end_property_grid();

  return modified;
}

bool OxUI::property(const char* label, Ref<TextureAsset>& texture, uint64_t overrideTextureID, const char* tooltip) {
  begin_property_grid(label, tooltip);
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

  vuk::SamplerCreateInfo sci;
  sci.minFilter = sci.magFilter = vuk::Filter::eLinear;
  sci.mipmapMode = vuk::SamplerMipmapMode::eLinear;
  sci.addressModeU = sci.addressModeV = sci.addressModeW = vuk::SamplerAddressMode::eRepeat;
  const vuk::SampledImage sampledImage(vuk::SampledImage::Global{.iv = *texture->get_texture().view, .sci = sci, .image_layout = vuk::ImageLayout::eShaderReadOnlyOptimal});

  if (ImGui::ImageButton(Application::get()->get_imgui_layer()->add_sampled_image(sampledImage), {buttonSize, buttonSize}, {1, 1}, {0, 0}, 0)) {
    const auto& path = FileDialogs::open_file({{"Texture file", "png,jpg"}});
    if (!path.empty()) {
      texture = AssetManager::get_texture_asset({path});
      changed = true;
    }
  }
  if (texture && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay)) {
    ImGui::BeginTooltip();
    ImGui::TextUnformatted(texture->get_path().c_str());
    ImGui::Spacing();
    ImGui::Image(Application::get()->get_imgui_layer()->add_sampled_image(sampledImage), {tooltipSize, tooltipSize}, {1, 1}, {0, 0});
    ImGui::EndTooltip();
  }
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
      const auto path = get_path_from_imgui_payload(payload);
      texture = AssetManager::get_texture_asset({path.string()});
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
    texture = TextureAsset::get_purple_texture();
    changed = true;
  }
  ImGui::PopStyleColor(3);
  ImGui::PopStyleVar();

  end_property_grid();
  return changed;
}

void OxUI::image(const vuk::Texture& texture, ImVec2 size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tintCol, const ImVec4& borderCol) {
  vuk::SamplerCreateInfo sci;
  sci.minFilter = sci.magFilter = vuk::Filter::eLinear;
  sci.mipmapMode = vuk::SamplerMipmapMode::eLinear;
  sci.addressModeU = sci.addressModeV = sci.addressModeW = vuk::SamplerAddressMode::eRepeat;
  vuk::SampledImage sampledImage(vuk::SampledImage::Global{.iv = *texture.view, .sci = sci, .image_layout = vuk::ImageLayout::eShaderReadOnlyOptimal});

  ImGui::Image(Application::get()->get_imgui_layer()->add_sampled_image(sampledImage), size, uv0, uv1, tintCol, borderCol);
}

void OxUI::image(vuk::SampledImage& texture, ImVec2 size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& tintCol, const ImVec4& borderCol) {
  ImGui::Image(Application::get()->get_imgui_layer()->add_sampled_image(texture), size, uv0, uv1, tintCol, borderCol);
}

bool OxUI::draw_vec3_control(const char* label, glm::vec3& values, const char* tooltip, float resetValue) {
  bool changed = false;

  begin_property_grid(label, tooltip, false);

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
    if (ImGui::Button("X", buttonSize)) {
      values.x = resetValue;
    }
    ImGui::PopFont();
    ImGui::PopStyleColor(4);

    ImGui::SameLine();
    if (ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f")) {
      changed = true;
    }
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
    if (ImGui::Button("Y", buttonSize)) {
      values.y = resetValue;
    }
    ImGui::PopFont();
    ImGui::PopStyleColor(4);

    ImGui::SameLine();
    if (ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f")) {
      changed = true;
    }
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
    if (ImGui::Button("Z", buttonSize)) {
      values.z = resetValue;
    }
    ImGui::PopFont();
    ImGui::PopStyleColor(4);

    ImGui::SameLine();
    if (ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f")) {
      changed = true;
    }
    ImGui::PopItemWidth();
    ImGui::PopStyleVar();
  }

  ImGui::PopStyleVar();

  end_property_grid();

  return changed;
}

bool OxUI::toggle_button(const char* label, bool state, ImVec2 size, float alpha, float pressedAlpha, ImGuiButtonFlags buttonFlags) {
  if (state) {
    ImVec4 color = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];

    color.w = pressedAlpha;
    ImGui::PushStyleColor(ImGuiCol_Button, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
  }
  else {
    ImVec4 color = ImGui::GetStyle().Colors[ImGuiCol_Button];
    ImVec4 hoveredColor = ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered];
    color.w = alpha;
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

ImVec2 OxUI::get_icon_button_size(const char8_t* icon, const char* label) {
  const float lineHeight = ImGui::GetTextLineHeight();
  const ImVec2 padding = ImGui::GetStyle().FramePadding;

  float width = ImGui::CalcTextSize(StringUtils::from_char8_t(icon)).x;
  width += ImGui::CalcTextSize(label).x;
  width += padding.x * 2.0f;

  return {width, lineHeight + padding.y * 2.0f};
}

bool OxUI::icon_button(const char8_t* icon, const char* label, ImVec4 iconColor) {
  ImGui::PushID(label);

  const float lineHeight = ImGui::GetTextLineHeight();
  const ImVec2 padding = ImGui::GetStyle().FramePadding;

  float width = ImGui::CalcTextSize(StringUtils::from_char8_t(icon)).x;
  width += ImGui::CalcTextSize(label).x;
  width += padding.x * 2.0f;
  float height = lineHeight + padding.y * 2.0f;

  const float cursorPosX = ImGui::GetCursorPosX();
  const bool clicked = ImGui::Button(label, {width, height});
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0, 0});
  ImGui::SameLine();
  ImGui::SetCursorPosX(cursorPosX);
  ImGui::TextColored(iconColor, "%s", StringUtils::from_char8_t(icon));
  ImGui::SameLine();
  ImGui::TextUnformatted(label);
  ImGui::PopStyleVar();
  ImGui::PopID();

  return clicked;
}

void OxUI::clipped_text(const ImVec2& pos_min, const ImVec2& pos_max, const char* text, const char* text_end, const ImVec2* text_size_if_known, const ImVec2& align, const ImRect* clip_rect, float wrap_width) {
  // Hide anything after a '##' string
  const char* text_display_end = ImGui::FindRenderedTextEnd(text, text_end);
  const int text_len = static_cast<int>(text_display_end - text);
  if (text_len == 0)
    return;

  ImGuiContext& g = *GImGui;
  ImGuiWindow* window = g.CurrentWindow;
  clipped_text(window->DrawList, pos_min, pos_max, text, text_display_end, text_size_if_known, align, clip_rect, wrap_width);
  if (g.LogEnabled)
    ImGui::LogRenderedText(&pos_min, text, text_display_end);
}

void OxUI::clipped_text(ImDrawList* draw_list, const ImVec2& pos_min, const ImVec2& pos_max, const char* text, const char* text_display_end, const ImVec2* text_size_if_known, const ImVec2& align, const ImRect* clip_rect, float wrap_width) {
  // Perform CPU side clipping for single clipped element to avoid using scissor state
  ImVec2 pos = pos_min;
  const ImVec2 text_size = text_size_if_known ? *text_size_if_known : ImGui::CalcTextSize(text, text_display_end, false, wrap_width);

  const ImVec2* clip_min = clip_rect ? &clip_rect->Min : &pos_min;
  const ImVec2* clip_max = clip_rect ? &clip_rect->Max : &pos_max;

  // Align whole block. We should defer that to the better rendering function when we'll have support for individual line alignment.
  if (align.x > 0.0f)
    pos.x = ImMax(pos.x, pos.x + (pos_max.x - pos.x - text_size.x) * align.x);

  if (align.y > 0.0f)
    pos.y = ImMax(pos.y, pos.y + (pos_max.y - pos.y - text_size.y) * align.y);

  // Render
  ImVec4 fine_clip_rect(clip_min->x, clip_min->y, clip_max->x, clip_max->y);
  draw_list->AddText(nullptr, 0.0f, pos, ImGui::GetColorU32(ImGuiCol_Text), text, text_display_end, wrap_width, &fine_clip_rect);
}

void OxUI::spacing(uint32_t count) {
  for (uint32_t i = 0; i < count; i++)
    ImGui::Spacing();
}

std::filesystem::path OxUI::get_path_from_imgui_payload(const ImGuiPayload* payload) {
  return std::string(static_cast<const char*>(payload->Data));
}

void OxUI::draw_gradient_shadow_bottom(float scale) {
  const auto draw_list = ImGui::GetWindowDrawList();
  const auto pos = ImGui::GetWindowPos();
  const auto window_height = ImGui::GetWindowHeight();
  const auto window_width = ImGui::GetWindowWidth();

  const ImRect bb(0, scale, pos.x + window_width, window_height + pos.y);
  draw_list->AddRectFilledMultiColor(bb.Min, bb.Max, IM_COL32(20, 20, 20, 0), IM_COL32(20, 20, 20, 0), IM_COL32(20, 20, 20, 255), IM_COL32(20, 20, 20, 255));
}

void OxUI::push_id() {
  ++ui_context_id;
  ImGui::PushID(ui_context_id);
  s_Counter = 0;
}

void OxUI::pop_id() {
  ImGui::PopID();
  --ui_context_id;
}

void OxUI::begin_property_grid(const char* label, const char* tooltip, bool rightAlignNextColumn) {
  push_id();

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
    ImGui::SetNextItemWidth(-1);

  id_buffer[0] = '#';
  id_buffer[1] = '#';
  memset(id_buffer + 2, 0, 14);
  ++s_Counter;
  const std::string buffer = fmt::format("##{}", s_Counter);
  std::memcpy(&id_buffer, buffer.data(), 16);
}

void OxUI::end_property_grid() {
  ImGui::PopID();
  pop_id();
}

void OxUI::center_next_window() {
  const auto center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
}

void OxUI::draw_framerate_overlay(const ImVec2 work_pos, const ImVec2 work_size, ImVec2 padding, bool* visible) {
  static int corner = 1;
  ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                                  ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                  ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
  if (corner != -1) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 window_pos, window_pos_pivot;
    window_pos.x = corner & 1 ? work_pos.x + work_size.x - padding.x : work_pos.x + padding.x;
    window_pos.y = corner & 2 ? work_pos.y + work_size.y - padding.y: work_pos.y + padding.y ;
    window_pos_pivot.x = corner & 1 ? 1.0f : 0.0f;
    window_pos_pivot.y = corner & 2 ? 1.0f : 0.0f;
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
    ImGui::SetNextWindowViewport(viewport->ID);
    window_flags |= ImGuiWindowFlags_NoMove;
  }
  ImGui::SetNextWindowBgAlpha(0.35f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 3.0f);
  if (ImGui::Begin("Performance Overlay", nullptr, window_flags)) {
    ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
  }
  if (ImGui::BeginPopupContextWindow()) {
    if (ImGui::MenuItem("Custom", nullptr, corner == -1))
      corner = -1;
    if (ImGui::MenuItem("Top-left", nullptr, corner == 0))
      corner = 0;
    if (ImGui::MenuItem("Top-right", nullptr, corner == 1))
      corner = 1;
    if (ImGui::MenuItem("Bottom-left", nullptr, corner == 2))
      corner = 2;
    if (ImGui::MenuItem("Bottom-right", nullptr, corner == 3))
      corner = 3;
    if (visible && *visible && ImGui::MenuItem("Close"))
      *visible = false;
    ImGui::EndPopup();
  }
  ImGui::End();
  ImGui::PopStyleVar();
}
}
