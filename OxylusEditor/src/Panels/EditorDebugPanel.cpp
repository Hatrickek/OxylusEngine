#include "EditorDebugPanel.h"
#include <icons/IconsMaterialDesignIcons.h>
#include <UI/OxUI.h>

#include "Assets/AssetManager.h"

namespace Ox {
  EditorDebugPanel::EditorDebugPanel() : EditorPanel("Editor Debug", ICON_MDI_BUG_OUTLINE) {}

  void EditorDebugPanel::on_imgui_render() {
    if (on_begin()) {
      if (ImGui::Button("Free unused assets")) {
        AssetManager::free_unused_assets();
      }
      on_end();
    }
  }
}
