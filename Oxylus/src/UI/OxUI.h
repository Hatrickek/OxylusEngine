#pragma once

#include <imgui.h>

namespace Oxylus {
  class OxUI {
  public:
    static constexpr auto TABLE_FLAGS = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp |
                                        ImGuiTableFlags_BordersInner | ImGuiTableFlags_BordersOuterH;
    // Begin table
    static void BeginUI(ImGuiTableFlags flags = TABLE_FLAGS);
    // End table
    static void EndUI();

    // Progress bar
    static void ProgressBar(const char* label, const float& value);
    // Text property 
    static void Property(const char* label, const char* fmt, ...);
    // Button
    static bool Button(const char* label, ImVec2 size = ImVec2(0.0f, 0.0f));
    // Centers the next window with given size.
    static void CenterNextWindow(ImVec2 windowSize);
  };
}
