#include "src/oxpch.h"

#include "Material.h"

#include "Core/Resources.h"
#include "Render/Vulkan/VulkanRenderer.h"

namespace Oxylus {
  VulkanDescriptorSet Material::s_DescriptorSet;

  void Material::Create(const std::string& name, const UUID& shaderID) {
    Name = name;

    //TODO:
    //Shader = VulkanRenderer::GetShaderByID(shaderID); 

    const auto& EmptyTexture = CreateRef<VulkanImage>(Resources::s_EngineResources.EmptyTexture);

    AlbedoTexture = EmptyTexture;
    NormalTexture = EmptyTexture;
    RoughnessTexture = EmptyTexture;
    MetallicTexture = EmptyTexture;
    AOTexture = EmptyTexture;

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
}
