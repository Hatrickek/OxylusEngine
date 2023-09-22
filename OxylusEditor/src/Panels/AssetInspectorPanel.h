#pragma once

#include "EditorPanel.h"
#include "Core/Base.h"

namespace Oxylus {
  class Material;

  class AssetInspectorPanel : public EditorPanel {
  public:
    AssetInspectorPanel();
    void OnUpdate() override;
    void OnImGuiRender() override;

  private:
    void DrawMaterialAsset(const std::string* path);

    Ref<Material> m_SelectedMaterial;
  };
}
