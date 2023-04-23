#include "src/oxpch.h"
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

  bool OxUI::Button(const char* label, const ImVec2 size) {
    //IGUI::BeginPropertyGrid(label, nullptr);
    const auto value = ImGui::Button(label, size);
    //IGUI::EndPropertyGrid();
    return value;
  }

  void OxUI::CenterNextWindow(ImVec2 windowSize) {
    windowSize.x *= 0.5f;
    windowSize.y *= 0.5f;
    ImGui::SetNextWindowPos(windowSize, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  }
}
