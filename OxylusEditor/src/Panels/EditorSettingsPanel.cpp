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
      const auto& ImGuiLayer = Application::Get()->GetImGuiLayer();
      const char* themes[] = {"Dark", "White"};
      if (ImGui::Combo("Theme", &ImGuiLayer->SelectedTheme, themes, OX_ARRAYSIZE(themes))) {
        ImGuiLayer->SetTheme(ImGuiLayer->SelectedTheme);
      }
      OnEnd();
    }
  }
}
