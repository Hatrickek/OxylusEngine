#include "AssetInspectorPanel.h"

#include <icons/IconsMaterialDesignIcons.h>

#include "EditorLayer.h"
#include "Assets/AssetManager.h"

namespace Oxylus {
  AssetInspectorPanel::AssetInspectorPanel() : EditorPanel("AssetInspector", ICON_MDI_INFORMATION) { }

  void AssetInspectorPanel::on_update() { }

  void AssetInspectorPanel::on_imgui_render() {
    if (on_begin()) {
      if (EditorContextType::Asset == EditorLayer::get()->GetContext().GetType()) {
        if (EditorLayer::get()->GetContext().GetAssetExtension() == ".oxmat") {
          DrawMaterialAsset(EditorLayer::get()->GetContext().As<std::string>());
        }
      }
      on_end();
    }
  }

  void AssetInspectorPanel::DrawMaterialAsset(const std::string* path) {
    //TODO:
    //m_SelectedMaterial = AssetManager::GetMaterialAsset(*path).Data;
    //InspectorPanel::DrawMaterialProperties(m_SelectedMaterial, true);
  }
}
