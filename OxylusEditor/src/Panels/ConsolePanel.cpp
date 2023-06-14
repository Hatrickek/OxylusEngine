#include "ConsolePanel.h"

#include <icons/IconsMaterialDesignIcons.h>

#include "Assets/AssetManager.h"

namespace Oxylus {
  ConsolePanel::ConsolePanel() {
    m_RuntimeConsole.RenderMenuBar = true;
    m_RuntimeConsole.PanelName = "Console";
    m_RuntimeConsole.RegisterCommand("clear_assets", [] { AssetManager::FreeUnusedAssets(); });
  }

  void ConsolePanel::OnImGuiRender() {
    m_RuntimeConsole.OnImGuiRender();
  }
}
