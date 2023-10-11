#include "EditorSettingsPanel.h"

#include "imgui.h"
#include "Core/Application.h"
#include "icons/IconsMaterialDesignIcons.h"

namespace Oxylus {
  EditorSettingsPanel::EditorSettingsPanel() : EditorPanel("Editor Settings", ICON_MDI_COGS, false) { }

  void EditorSettingsPanel::OnImGuiRender() {
    constexpr ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
    if (OnBegin(window_flags)) {
      //Theme
      const char* themes[] = {"Dark", "White"};
      int themeIndex = 0;
      if (ImGui::Combo("Theme", &themeIndex, themes, OX_ARRAYSIZE(themes))) {
        ImGuiLayer::apply_theme(!(bool)themeIndex);
      }
      OnEnd();
    }
  }
}
