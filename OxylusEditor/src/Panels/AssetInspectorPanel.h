#pragma once

#include "EditorPanel.h"
#include "Core/Base.h"

namespace Oxylus {
  class Material;

  class AssetInspectorPanel : public EditorPanel {
  public:
    AssetInspectorPanel();
    void on_update() override;
    void on_imgui_render() override;

  private:
    void DrawMaterialAsset(const std::string* path);

    Shared<Material> m_SelectedMaterial;
  };
}
