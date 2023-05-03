#include "src/oxpch.h"

#include "Material.h"

#include "Core/Resources.h"
#include "Render/Vulkan/VulkanRenderer.h"

namespace Oxylus {
  VulkanDescriptorSet Material::s_DescriptorSet;

  Material::~Material() { }

  void Material::Create(const std::string& name, const UUID& shaderID) {
    ZoneScoped;
    Name = name;

    //TODO:
    //Shader = VulkanRenderer::GetShaderByID(shaderID); 

    ClearTextures();

    if (!s_DescriptorSet.Get())
      s_DescriptorSet.CreateFromPipeline(VulkanRenderer::s_Pipelines.PBRPipeline);
    MaterialDescriptorSet.CreateFromPipeline(VulkanRenderer::s_Pipelines.PBRPipeline, 1);

    s_DescriptorSet.WriteDescriptorSets[9].pImageInfo = &VulkanRenderer::s_FrameBuffers.DepthNormalPassFB.GetImage()[0].GetDescImageInfo();
    s_DescriptorSet.WriteDescriptorSets[10].pImageInfo = &VulkanRenderer::s_Resources.DirectShadowsDepthArray.GetDescImageInfo();
    s_DescriptorSet.Update(true);

    MaterialDescriptorSet.WriteDescriptorSets[0].pImageInfo = &AlbedoTexture->GetDescImageInfo();
    MaterialDescriptorSet.WriteDescriptorSets[1].pImageInfo = &NormalTexture->GetDescImageInfo();
    MaterialDescriptorSet.WriteDescriptorSets[2].pImageInfo = &AOTexture->GetDescImageInfo();
    MaterialDescriptorSet.WriteDescriptorSets[3].pImageInfo = &MetallicTexture->GetDescImageInfo();
    MaterialDescriptorSet.WriteDescriptorSets[4].pImageInfo = &RoughnessTexture->GetDescImageInfo();
    MaterialDescriptorSet.Update(true);
  }

  bool Material::IsOpaque() const {
    return AlphaMode == AlphaMode::Opaque;
  }

  void Material::Update() {
    ZoneScoped;
    s_DescriptorSet.WriteDescriptorSets[9].pImageInfo = &VulkanRenderer::s_FrameBuffers.DepthNormalPassFB.GetImage()[0].GetDescImageInfo();
    s_DescriptorSet.WriteDescriptorSets[10].pImageInfo = &VulkanRenderer::s_Resources.DirectShadowsDepthArray.GetDescImageInfo();
    s_DescriptorSet.Update(true);

    MaterialDescriptorSet.WriteDescriptorSets[0].pImageInfo = &AlbedoTexture->GetDescImageInfo();
    MaterialDescriptorSet.WriteDescriptorSets[1].pImageInfo = &NormalTexture->GetDescImageInfo();
    MaterialDescriptorSet.WriteDescriptorSets[2].pImageInfo = &AOTexture->GetDescImageInfo();
    MaterialDescriptorSet.WriteDescriptorSets[3].pImageInfo = &MetallicTexture->GetDescImageInfo();
    MaterialDescriptorSet.WriteDescriptorSets[4].pImageInfo = &RoughnessTexture->GetDescImageInfo();
    MaterialDescriptorSet.Update(true);
  }

  void Material::Destroy() {
    if (MaterialDescriptorSet.Get())
      MaterialDescriptorSet.Destroy();
    ClearTextures();
    Parameters = {};
  }

  void Material::ClearTextures() {
    const auto& EmptyTexture = CreateRef<VulkanImage>(Resources::s_EngineResources.EmptyTexture);

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
