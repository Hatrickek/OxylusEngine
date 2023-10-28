#pragma once

#include <cstdint>
#include <filesystem>

#include <imgui.h>
#include <imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>

#include "Core/Base.h"

namespace vuk {
struct SampledImage;
struct Texture;
}

namespace Oxylus {
class TextureAsset;

class OxUI {
public:
  static void begin_properties(ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchSame |
                                                       ImGuiTableFlags_BordersInner | ImGuiTableFlags_BordersOuterH);

  static void end_properties();

  // Text
  static void text(const char* text1, const char* text2, const char* tooltip = nullptr);

  // Bool
  static bool property(const char* label, bool* flag, const char* tooltip = nullptr);

  // InputField
  static bool property(const char* label, std::string* text, ImGuiInputFlags flags, const char* tooltip = nullptr);

  // Dropdown
  static bool property(const char* label,
                       int* value,
                       const char** dropdownStrings,
                       int count,
                       const char* tooltip = nullptr);

  template <std::integral T>
  static bool property(const char* label,
                       T* value,
                       T min = 0,
                       T max = 0,
                       const char* tooltip = nullptr) {
    begin_property_grid(label, tooltip);
    bool modified;

    int dataType = ImGuiDataType_S32;
    if constexpr (std::is_signed_v<T>) {
      if constexpr (sizeof(T) == 1)
        dataType = ImGuiDataType_S8;
      else if constexpr (sizeof(T) == 2)
        dataType = ImGuiDataType_S16;
      else if constexpr (sizeof(T) == 4)
        dataType = ImGuiDataType_S32;
      else if constexpr (sizeof(T) == 8)
        dataType = ImGuiDataType_S64;
    }
    else {
      if constexpr (sizeof(T) == 1)
        dataType = ImGuiDataType_U8;
      else if constexpr (sizeof(T) == 2)
        dataType = ImGuiDataType_U16;
      else if constexpr (sizeof(T) == 4)
        dataType = ImGuiDataType_U32;
      else if constexpr (sizeof(T) == 8)
        dataType = ImGuiDataType_U64;
    }

    if (max > min)
      modified = ImGui::SliderScalar(id_buffer, dataType, value, &min, &max);
    else
      modified = ImGui::DragScalar(id_buffer, dataType, value);

    end_property_grid();
    return modified;
  }

  template <std::floating_point T>
  static bool property(const char* label,
                       T* value,
                       T min = 0,
                       T max = 0,
                       const char* tooltip = nullptr,
                       float delta = 0.1f,
                       const char* fmt = "%.3f") {
    begin_property_grid(label, tooltip);
    bool modified;

    int dataType = ImGuiDataType_Float;
    if constexpr (sizeof(T) == 8)
      dataType = ImGuiDataType_Double;

    if (max > min)
      modified = ImGui::SliderScalar(id_buffer, dataType, value, &min, &max, fmt);
    else
      modified = ImGui::DragScalar(id_buffer, dataType, value, delta, nullptr, nullptr, fmt);

    end_property_grid();
    return modified;
  }

  // Vec2/3/4
  template <typename T>
  static bool property_vector(const char* label,
                             T& value,
                             bool color = false,
                             bool showAlpha = true,
                             const char* tooltip = nullptr,
                             float delta = 0.1f) {
    begin_property_grid(label, tooltip);
    bool modified;
    int componentCount = value.length();
    if (componentCount >= 3 && color) {
      if (showAlpha)
        modified = ImGui::ColorEdit4(id_buffer, glm::value_ptr(value));
      else
        modified = ImGui::ColorEdit3(id_buffer, glm::value_ptr(value));
    }
    else {
      modified = ImGui::DragScalarN(id_buffer, ImGuiDataType_Float, glm::value_ptr(value), componentCount, delta);
    }
    end_property_grid();
    return modified;
  }

  // Texture
  static bool property(const char* label,
                       Ref<TextureAsset>& texture,
                       uint64_t overrideTextureID = 0,
                       const char* tooltip = nullptr);
  // Draw vuk::Texture
  static void image(const vuk::Texture& texture,
                    ImVec2 size,
                    const ImVec2& uv0 = ImVec2(0, 0),
                    const ImVec2& uv1 = ImVec2(1, 1),
                    const ImVec4& tintCol = ImVec4(1, 1, 1, 1),
                    const ImVec4& borderCol = ImVec4(0, 0, 0, 0));

  static void image(vuk::SampledImage& texture,
                    ImVec2 size,
                    const ImVec2& uv0 = ImVec2(0, 0),
                    const ImVec2& uv1 = ImVec2(1, 1),
                    const ImVec4& tintCol = ImVec4(1, 1, 1, 1),
                    const ImVec4& borderCol = ImVec4(0, 0, 0, 0));

  // Vec3 with reset button
  static bool draw_vec3_control(const char* label,
                                glm::vec3& values,
                                const char* tooltip = nullptr,
                                float resetValue = 0.0f);

  static bool toggle_button(const char* label,
                            bool state,
                            ImVec2 size = {0, 0},
                            float alpha = 1.0f,
                            float pressedAlpha = 1.0f,
                            ImGuiButtonFlags buttonFlags = ImGuiButtonFlags_None);

  static ImVec2 get_icon_button_size(const char8_t* icon, const char* label);

  static bool icon_button(const char8_t* icon, const char* label, ImVec4 iconColor = {0.537f, 0.753f, 0.286f, 1.0f});
  static void clipped_text(const ImVec2& pos_min,
                           const ImVec2& pos_max,
                           const char* text,
                           const char* text_end,
                           const ImVec2* text_size_if_known,
                           const ImVec2& align,
                           const ImRect* clip_rect,
                           float wrap_width);

  static void clipped_text(ImDrawList* draw_list,
                           const ImVec2& pos_min,
                           const ImVec2& pos_max,
                           const char* text,
                           const char* text_display_end,
                           const ImVec2* text_size_if_known,
                           const ImVec2& align,
                           const ImRect* clip_rect,
                           float wrap_width);

  static std::filesystem::path get_path_from_imgui_payload(const ImGuiPayload* payload);

  static void draw_gradient_shadow();

  static void begin_property_grid(const char* label, const char* tooltip, bool rightAlignNextColumn = true);

  static void end_property_grid();

  static void CenterNextWindow();

  static void push_id();

  static void pop_id();

  static char id_buffer[16];
};
}
