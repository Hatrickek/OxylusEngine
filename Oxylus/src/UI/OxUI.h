#pragma once
#include <imgui.h>

namespace Oxylus {
  class OxUI {
  public:
    static constexpr auto TABLE_FLAGS = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp |
      ImGuiTableFlags_BordersInner | ImGuiTableFlags_BordersOuterH;
    static void BeginUI(ImGuiTableFlags flags = TABLE_FLAGS);
    static void EndUI();

    //Progress bar
    static void ProgressBar(const char* label, const float& value);
    static void Property(const char* label, const char* fmt, ...);
  };
}
