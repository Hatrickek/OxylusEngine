#include "ShadersPanel.h"

#include <imgui.h>

#include <icons/IconsMaterialDesignIcons.h>

#include "Render/RendererConfig.h"

namespace Oxylus {
  ShadersPanel::ShadersPanel() : EditorPanel("Shaders", ICON_MDI_FILE_CHART, false) { }

  void ShadersPanel::on_imgui_render() {
    if (on_begin()) {
      if (ImGui::Button("Reload render pipeline")) {
        RendererCVar::cvar_reload_render_pipeline.toggle();
      }
      on_end();
    }
  }
}
