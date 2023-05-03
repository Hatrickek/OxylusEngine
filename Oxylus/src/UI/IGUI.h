#pragma once

#include <cstdint>
#include <filesystem>

#include <imgui.h>
#include <imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>
#include "Render/Vulkan/VulkanImage.h"

namespace Oxylus {
  class IGUI {
  public:
    static void BeginProperties(
      ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp |
                              ImGuiTableFlags_BordersInner | ImGuiTableFlags_BordersOuterH);

    static void EndProperties();

    // Bool
    static bool Property(const char* label, bool& flag, const char* tooltip = nullptr);

    // Dropdown
    static bool Property(const char* label,
                         int& value,
                         const char** dropdownStrings,
                         int count,
                         const char* tooltip = nullptr);

    template <std::integral T>
    static bool Property(const char* label,
                         T& value,
                         T min = 0,
                         T max = 0,
                         const char* tooltip = nullptr) {
      BeginPropertyGrid(label, tooltip);
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
        modified = ImGui::SliderScalar(s_IDBuffer, dataType, &value, &min, &max);
      else
        modified = ImGui::DragScalar(s_IDBuffer, dataType, &value);

      EndPropertyGrid();
      return modified;
    }

    template <std::floating_point T>
    static bool Property(const char* label,
                         T& value,
                         T min = 0,
                         T max = 0,
                         const char* tooltip = nullptr,
                         float delta = 0.1f,
                         const char* fmt = "%.3f") {
      BeginPropertyGrid(label, tooltip);
      bool modified;

      int dataType = ImGuiDataType_Float;
      if constexpr (sizeof(T) == 8)
        dataType = ImGuiDataType_Double;

      if (max > min)
        modified = ImGui::SliderScalar(s_IDBuffer, dataType, &value, &min, &max, fmt);
      else
        modified = ImGui::DragScalar(s_IDBuffer, dataType, &value, delta, nullptr, nullptr, fmt);

      EndPropertyGrid();
      return modified;
    }

    // Vec2/3/4
    template <typename T>
    static bool PropertyVector(const char* label,
                               T& value,
                               bool color = false,
                               bool showAlpha = true,
                               const char* tooltip = nullptr,
                               float delta = 0.1f) {
      BeginPropertyGrid(label, tooltip);
      bool modified;
      int componentCount = value.length();
      if (componentCount >= 3 && color) {
        if (showAlpha)
          modified = ImGui::ColorEdit4(s_IDBuffer, glm::value_ptr(value));
        else
          modified = ImGui::ColorEdit3(s_IDBuffer, glm::value_ptr(value));
      }
      else {
        modified = ImGui::DragScalarN(s_IDBuffer, ImGuiDataType_Float, glm::value_ptr(value), componentCount, delta);
      }
      EndPropertyGrid();
      return modified;
    }

    //Texture
    static bool Property(const char* label,
                         Ref<VulkanImage>& texture,
                         uint64_t overrideTextureID = 0,
                         const char* tooltip = nullptr);

    // Vec3 with reset button
    static void DrawVec3Control(const char* label,
                                glm::vec3& values,
                                const char* tooltip = nullptr,
                                float resetValue = 0.0f);

    static bool ToggleButton(const char* label,
                             bool state,
                             ImVec4 defaultColor = ImVec4(),
                             ImVec2 size = {0, 0},
                             float pressedAlpha = 1.0f,
                             ImGuiButtonFlags buttonFlags = ImGuiButtonFlags_None);

    static bool ToggleButton(const char* label, bool state, ImVec2 size = { 0, 0 }, float alpha = 1.0f, float pressedAlpha = 1.0f, ImGuiButtonFlags buttonFlags = ImGuiButtonFlags_None);

    static ImVec2 GetIconButtonSize(const char8_t* icon, const char* label);

    static bool IconButton(const char8_t* icon, const char* label, ImVec4 iconColor = {0.537f, 0.753f, 0.286f, 1.0f});
    static void ClippedText(const ImVec2& pos_min,
                            const ImVec2& pos_max,
                            const char* text,
                            const char* text_end,
                            const ImVec2* text_size_if_known,
                            const ImVec2& align,
                            const ImRect* clip_rect,
                            float wrap_width);

    static void ClippedText(ImDrawList* draw_list,
                            const ImVec2& pos_min,
                            const ImVec2& pos_max,
                            const char* text,
                            const char* text_display_end,
                            const ImVec2* text_size_if_known,
                            const ImVec2& align,
                            const ImRect* clip_rect,
                            float wrap_width);
    static std::filesystem::path GetPathFromImGuiPayload(const ImGuiPayload* payload);

    static void BeginPropertyGrid(const char* label, const char* tooltip, bool rightAlignNextColumn = true);

    static void EndPropertyGrid();

    static void PushID();

    static void PopID();

    static char s_IDBuffer[16];
  };
}
