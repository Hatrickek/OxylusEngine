#include "EditorSettingsPanel.h"

#include "imgui.h"
#include "Core/Application.h"
#include "Utils/EditorConfig.h"
#include "Utils/ImGuiScoped.h"
#include "icons/IconsMaterialDesignIcons.h"

namespace Oxylus {
  EditorSettingsPanel::EditorSettingsPanel() : EditorPanel("Editor Settings", ICON_MDI_COGS, false) { }

  void EditorSettingsPanel::OnImGuiRender() {
    constexpr ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
    OnBegin(window_flags);
    {
      //Theme
      const auto& ImGuiLayer = Application::Get().GetImGuiLayer();
      const char* themes[] = {"Dark", "Dark 2", "White"};
      if (ImGui::Combo("Theme", &ImGuiLayer->SelectedTheme, themes, OX_ARRAYSIZE(themes))) {
        ImGuiLayer->SetTheme(ImGuiLayer->SelectedTheme);
      }
    }
    static bool windowDecors; //TODO:
    ImGui::Checkbox("Custom window titlebar", &windowDecors);
    OnEnd();
  }
}
