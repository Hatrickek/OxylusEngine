#include "ConsolePanel.h"

#include <icons/IconsMaterialDesignIcons.h>

#include "Assets/AssetManager.h"

namespace Oxylus {
  ConsolePanel::ConsolePanel() {
    m_RuntimeConsole.RenderMenuBar = true;
    m_RuntimeConsole.PanelName = "Console";
    m_RuntimeConsole.RegisterCommand("clear_assets", "Asset cleared.", [] { AssetManager::free_unused_assets(); });
  }

  void ConsolePanel::OnImGuiRender() {
    m_RuntimeConsole.OnImGuiRender();
  }
}
