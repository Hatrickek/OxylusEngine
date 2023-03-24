#include "AssetInspectorPanel.h"

#include <format>
#include <imgui.h>
#include <icons/IconsMaterialDesignIcons.h>

#include <Assets/MaterialSerializer.h>
#include <UI/IGUI.h>
#include <Utils/StringUtils.h>

#include "EditorLayer.h"

namespace Oxylus {
  AssetInspectorPanel::AssetInspectorPanel() : EditorPanel("AssetInspector", ICON_MDI_INFORMATION) { }

  void AssetInspectorPanel::OnUpdate() {
    if (!((m_SelectedAsset = EditorLayer::Get()->GetSelectedAsset()))) {
      Visible = false;
      InvalidateSerializedAssets();
    }
  }

  void AssetInspectorPanel::OnImGuiRender() {
    if (OnBegin()) {
      if (m_SelectedAsset) {
        DrawAssets(m_SelectedAsset);
      }
      OnEnd();
    }
  }

  void AssetInspectorPanel::HandleMaterialAsset(const EditorAsset& asset) {
    if (!m_DeserializedMaterial) {
      m_DeserializedMaterial = CreateRef<Material>();
      m_DeserializedMaterial->Create();
      const MaterialSerializer serializer(*m_DeserializedMaterial);
      serializer.Deserialize(asset.Path);
    }
    else {
      if (DrawMaterialProperties(m_DeserializedMaterial)) {
        const MaterialSerializer serializer(*m_DeserializedMaterial);
        serializer.Serialize(asset.Path);
      }
    }
  }

  void AssetInspectorPanel::DrawAssets(const EditorAsset& asset) {
    switch (asset.Type) {
      case AssetType::None: break;
      case AssetType::Mesh: break;
      case AssetType::Sound: break;
      case AssetType::Image: break;
      case AssetType::Material: {
        HandleMaterialAsset(asset);
        break;
      }
    }
  }

  bool AssetInspectorPanel::DrawMaterialProperties(const Ref<Material>& material) const {
    IGUI::BeginProperties();

    bool shouldUpdate = false;

    IGUI::Property("Use Albedo", (bool&)material->Parameters.UseAlbedo);
    if (IGUI::Property("Albedo", material->AlbedoTexture)) {
      material->Update();
      shouldUpdate = true;
    }
    if (IGUI::PropertyVector("Color", material->Parameters.Color, true, true))
      shouldUpdate = true;

    if (IGUI::Property("Specular", material->Parameters.Specular))
      shouldUpdate = true;

    IGUI::Property("Use Normal", (bool&)material->Parameters.UseNormal);
    if (IGUI::Property("Normal", material->NormalTexture)) {
      material->Update();
      shouldUpdate = true;
    }

    IGUI::Property("Use Roughness", (bool&)material->Parameters.UseRoughness);
    if (IGUI::Property("Roughness", material->RoughnessTexture)) {
      material->Update();
      shouldUpdate = true;
    }
    if (IGUI::Property("Roughness", material->Parameters.Roughness))
      shouldUpdate = true;

    IGUI::Property("Use Metallic", (bool&)material->Parameters.UseMetallic);
    if (IGUI::Property("Metallic", material->MetallicTexture)) {
      material->Update();
      shouldUpdate = true;
    }

    IGUI::Property("Use AO", (bool&)material->Parameters.UseAO);
    if (IGUI::Property("AO", material->AOTexture)) {
      material->Update();
      shouldUpdate = true;
    }
    IGUI::EndProperties();

    return shouldUpdate;
  }

  void AssetInspectorPanel::InvalidateSerializedAssets() {
    m_DeserializedMaterial.reset();
  }
}
