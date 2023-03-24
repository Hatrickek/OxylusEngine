#pragma once

#include <Assets/Assets.h>

#include "EditorPanel.h"

namespace Oxylus {
  class Material;

  class AssetInspectorPanel : public EditorPanel {
  public:
    AssetInspectorPanel();
    void OnUpdate() override;
    void OnImGuiRender() override;

  private:
    void DrawAssets(const EditorAsset& asset);
    bool DrawMaterialProperties(const Ref<Material>& material) const;
    void HandleMaterialAsset(const EditorAsset& asset);
    void InvalidateSerializedAssets();

    EditorAsset m_SelectedAsset{};

    Ref<Material> m_DeserializedMaterial;
  };
}
