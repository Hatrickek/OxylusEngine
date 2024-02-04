#include "ConsolePanel.h"

#include "Assets/AssetManager.h"

namespace Ox {
ConsolePanel::ConsolePanel() {
  runtime_console.render_menu_bar = true;
  runtime_console.panel_name = "Console";
  runtime_console.register_command("clear_assets", "Asset cleared.", [] { AssetManager::free_unused_assets(); });
}

void ConsolePanel::on_imgui_render() {
  runtime_console.on_imgui_render();
}
}
