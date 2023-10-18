#include "EditorDebugPanel.h"
#include <icons/IconsMaterialDesignIcons.h>
#include <UI/IGUI.h>

#include "Assets/AssetManager.h"

namespace Oxylus {
  EditorDebugPanel::EditorDebugPanel() : EditorPanel("Editor Debug", ICON_MDI_BUG_OUTLINE) {}

  void EditorDebugPanel::on_imgui_render() {
    if (OnBegin()) {
      if (ImGui::Button("Free unused assets")) {
        AssetManager::free_unused_assets();
      }
      OnEnd();
    }
  }
}
