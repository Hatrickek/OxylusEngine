#include "OxUI.h"
#include "IGUI.h"

namespace Oxylus {
void OxUI::BeginUI(ImGuiTableFlags flags) {
  IGUI::begin_properties(flags);
}

void OxUI::EndUI() {
  IGUI::end_properties();
}

void OxUI::ProgressBar(const char* label, const float& value) {
  IGUI::begin_property_grid(label, nullptr);
  ImGui::ProgressBar(value, ImVec2(0.0f, 0.0f));
  IGUI::end_property_grid();
}

void OxUI::Property(const char* label, const char* fmt, ...) {
  IGUI::begin_property_grid(label, nullptr);
  va_list args;
  va_start(args, fmt);
  ImGui::TextV(fmt, args);
  va_end(args);
  IGUI::end_property_grid();
}

bool OxUI::Button(const char* label, const ImVec2 size) {
  //IGUI::BeginPropertyGrid(label, nullptr);
  const auto value = ImGui::Button(label, size);
  //IGUI::EndPropertyGrid();
  return value;
}

void OxUI::CenterNextWindow() {
  const auto center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
}
}
