#include "Material.h"

#include "Core/Resources.h"
#include "Render/ShaderLibrary.h"
#include "Render/Vulkan/VulkanRenderer.h"

namespace Oxylus {
  VulkanDescriptorSet Material::s_DescriptorSet;

  Material::~Material() { }

  void Material::Create(const std::string& name) {
    OX_SCOPED_ZONE;
    Name = name;

    m_Shader = ShaderLibrary::GetShader("PBRTiled");

    ClearTextures();

    MaterialDescriptorSet.CreateFromShader(m_Shader, 1);
    DepthDescriptorSet.CreateFromShader(ShaderLibrary::GetShader("DepthPass"), 1);

    MaterialDescriptorSet.WriteDescriptorSets[0].pImageInfo = &AlbedoTexture->GetDescImageInfo();
    MaterialDescriptorSet.WriteDescriptorSets[1].pImageInfo = &NormalTexture->GetDescImageInfo();
    MaterialDescriptorSet.WriteDescriptorSets[2].pImageInfo = &AOTexture->GetDescImageInfo();
    MaterialDescriptorSet.WriteDescriptorSets[3].pImageInfo = &MetallicTexture->GetDescImageInfo();
    MaterialDescriptorSet.WriteDescriptorSets[4].pImageInfo = &RoughnessTexture->GetDescImageInfo();
    MaterialDescriptorSet.Update(true);

    DepthDescriptorSet.WriteDescriptorSets[0].pImageInfo = &NormalTexture->GetDescImageInfo();
    DepthDescriptorSet.WriteDescriptorSets[1].pImageInfo = &RoughnessTexture->GetDescImageInfo();
    DepthDescriptorSet.Update(true);
  }

  bool Material::IsOpaque() const {
    return AlphaMode == AlphaMode::Opaque;
  }

  void Material::Update() {
    OX_SCOPED_ZONE;
    MaterialDescriptorSet.WriteDescriptorSets[0].pImageInfo = &AlbedoTexture->GetDescImageInfo();
    MaterialDescriptorSet.WriteDescriptorSets[1].pImageInfo = &NormalTexture->GetDescImageInfo();
    MaterialDescriptorSet.WriteDescriptorSets[2].pImageInfo = &AOTexture->GetDescImageInfo();
    MaterialDescriptorSet.WriteDescriptorSets[3].pImageInfo = &MetallicTexture->GetDescImageInfo();
    MaterialDescriptorSet.WriteDescriptorSets[4].pImageInfo = &RoughnessTexture->GetDescImageInfo();
    MaterialDescriptorSet.Update(true);

    DepthDescriptorSet.WriteDescriptorSets[0].pImageInfo = &NormalTexture->GetDescImageInfo();
    DepthDescriptorSet.WriteDescriptorSets[1].pImageInfo = &RoughnessTexture->GetDescImageInfo();
    DepthDescriptorSet.Update(true);
  }

  void Material::Destroy() {
    ClearTextures();
    Parameters = {};
  }

  void Material::ClearTextures() {
    const auto& EmptyTexture = Resources::s_EngineResources.EmptyTexture;

    AlbedoTexture = EmptyTexture;
    NormalTexture = EmptyTexture;
    RoughnessTexture = EmptyTexture;
    MetallicTexture = EmptyTexture;
    AOTexture = EmptyTexture;
    EmissiveTexture = EmptyTexture;
    SpecularTexture = EmptyTexture;
    DiffuseTexture = EmptyTexture;
  }
}
