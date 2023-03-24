#include "oxpch.h"
#include "OxUI.h"
#include "IGUI.h"

namespace Oxylus {
  void OxUI::BeginUI(ImGuiTableFlags flags) {
    IGUI::BeginProperties(flags);
  }

  void OxUI::EndUI() {
    IGUI::EndProperties();
  }

  void OxUI::ProgressBar(const char* label, const float& value) {
    IGUI::BeginPropertyGrid(label, nullptr);
    ImGui::ProgressBar(value, ImVec2(0.0f, 0.0f));
    IGUI::EndPropertyGrid();
  }

  void OxUI::Property(const char* label, const char* fmt, ...) {
    IGUI::BeginPropertyGrid(label, nullptr);
    va_list args;
    va_start(args, fmt);
    ImGui::TextV(fmt, args);
    va_end(args);
    IGUI::EndPropertyGrid();
  }
}
