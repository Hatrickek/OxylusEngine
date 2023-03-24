#include "oxpch.h"

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
      s_DescriptorSet.Allocate(VulkanRenderer::s_Pipelines.PBRPipeline.GetDescriptorSetLayout());
    MaterialDescriptorSet.Allocate(VulkanRenderer::s_Pipelines.PBRPipeline.GetDescriptorSetLayout(), 1);

    s_DescriptorSet.WriteDescriptorSets = {
      {
        s_DescriptorSet.Get(), 0, 0, 1, vDT::eUniformBuffer, nullptr,
        &VulkanRenderer::s_RendererData.ObjectsBuffer.GetDescriptor()
      },
      {
        s_DescriptorSet.Get(), 1, 0, 1, vDT::eUniformBuffer, nullptr,
        &VulkanRenderer::s_RendererData.ParametersBuffer.GetDescriptor()
      },
      {
        s_DescriptorSet.Get(), 1, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr,
        &VulkanRenderer::s_RendererData.ParametersBuffer.GetDescriptor()
      },
      {
        s_DescriptorSet.Get(), 2, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr,
        &VulkanRenderer::s_RendererData.LightsBuffer.GetDescriptor()
      },
      {
        s_DescriptorSet.Get(), 3, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr,
        &VulkanRenderer::s_RendererData.FrustumBuffer.GetDescriptor()
      },
      {
        s_DescriptorSet.Get(), 4, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr,
        &VulkanRenderer::s_RendererData.LighIndexBuffer.GetDescriptor()
      },
      {
        s_DescriptorSet.Get(), 5, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr,
        &VulkanRenderer::s_RendererData.LighGridBuffer.GetDescriptor()
      },
      {
        s_DescriptorSet.Get(), 6, 0, 1, vDT::eCombinedImageSampler,
        &VulkanRenderer::s_Resources.IrradianceCube.GetDescImageInfo()
      },
      {
        s_DescriptorSet.Get(), 7, 0, 1, vDT::eCombinedImageSampler,
        &VulkanRenderer::s_Resources.LutBRDF.GetDescImageInfo()
      },
      {
        s_DescriptorSet.Get(), 8, 0, 1, vDT::eCombinedImageSampler,
        &VulkanRenderer::s_Resources.PrefilteredCube.GetDescImageInfo()
      },
      {
        s_DescriptorSet.Get(), 9, 0, 1, vDT::eCombinedImageSampler,
        &VulkanRenderer::s_FrameBuffers.DepthNormalPassFB.GetImage()[0].GetDescImageInfo()
      },
      {
        s_DescriptorSet.Get(), 10, 0, 1, vDT::eCombinedImageSampler,
        &VulkanRenderer::s_Resources.DirectShadowsDepthArray.GetDescImageInfo()
      },
      {
        s_DescriptorSet.Get(), 11, 0, 1, vDT::eUniformBuffer, nullptr,
        &VulkanRenderer::s_RendererData.DirectShadowBuffer.GetDescriptor()
      },
    };
    s_DescriptorSet.Update(true);

    MaterialDescriptorSet.WriteDescriptorSets = {
      {MaterialDescriptorSet.Get(), 0, 0, 1, vDT::eCombinedImageSampler, &AlbedoTexture->GetDescImageInfo()},
      {MaterialDescriptorSet.Get(), 1, 0, 1, vDT::eCombinedImageSampler, &NormalTexture->GetDescImageInfo()},
      {MaterialDescriptorSet.Get(), 2, 0, 1, vDT::eCombinedImageSampler, &AOTexture->GetDescImageInfo()},
      {MaterialDescriptorSet.Get(), 3, 0, 1, vDT::eCombinedImageSampler, &MetallicTexture->GetDescImageInfo()},
      {MaterialDescriptorSet.Get(), 4, 0, 1, vDT::eCombinedImageSampler, &RoughnessTexture->GetDescImageInfo()},
    };
    MaterialDescriptorSet.Update(true);
  }

  bool Material::IsOpaque() const {
    return AlphaMode == AlphaMode::Opaque;
  }

  void Material::Update() {
    s_DescriptorSet.WriteDescriptorSets = {
      {
        s_DescriptorSet.Get(), 0, 0, 1, vDT::eUniformBuffer, nullptr,
        &VulkanRenderer::s_RendererData.ObjectsBuffer.GetDescriptor()
      },
      {
        s_DescriptorSet.Get(), 1, 0, 1, vDT::eUniformBuffer, nullptr,
        &VulkanRenderer::s_RendererData.ParametersBuffer.GetDescriptor()
      },
      {
        s_DescriptorSet.Get(), 1, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr,
        &VulkanRenderer::s_RendererData.ParametersBuffer.GetDescriptor()
      },
      {
        s_DescriptorSet.Get(), 2, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr,
        &VulkanRenderer::s_RendererData.LightsBuffer.GetDescriptor()
      },
      {
        s_DescriptorSet.Get(), 3, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr,
        &VulkanRenderer::s_RendererData.FrustumBuffer.GetDescriptor()
      },
      {
        s_DescriptorSet.Get(), 4, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr,
        &VulkanRenderer::s_RendererData.LighIndexBuffer.GetDescriptor()
      },
      {
        s_DescriptorSet.Get(), 5, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr,
        &VulkanRenderer::s_RendererData.LighGridBuffer.GetDescriptor()
      },
      {
        s_DescriptorSet.Get(), 6, 0, 1, vDT::eCombinedImageSampler,
        &VulkanRenderer::s_Resources.IrradianceCube.GetDescImageInfo()
      },
      {
        s_DescriptorSet.Get(), 7, 0, 1, vDT::eCombinedImageSampler,
        &VulkanRenderer::s_Resources.LutBRDF.GetDescImageInfo()
      },
      {
        s_DescriptorSet.Get(), 8, 0, 1, vDT::eCombinedImageSampler,
        &VulkanRenderer::s_Resources.PrefilteredCube.GetDescImageInfo()
      },
      {
        s_DescriptorSet.Get(), 9, 0, 1, vDT::eCombinedImageSampler,
        &VulkanRenderer::s_FrameBuffers.DepthNormalPassFB.GetImage()[0].GetDescImageInfo()
      },
      {
        s_DescriptorSet.Get(), 10, 0, 1, vDT::eCombinedImageSampler,
        &VulkanRenderer::s_Resources.DirectShadowsDepthArray.GetDescImageInfo()
      },
      {
        s_DescriptorSet.Get(), 11, 0, 1, vDT::eUniformBuffer, nullptr,
        &VulkanRenderer::s_RendererData.DirectShadowBuffer.GetDescriptor()
      },
    };
    s_DescriptorSet.Update(true);

    MaterialDescriptorSet.WriteDescriptorSets = {
      {MaterialDescriptorSet.Get(), 0, 0, 1, vDT::eCombinedImageSampler, &AlbedoTexture->GetDescImageInfo()},
      {MaterialDescriptorSet.Get(), 1, 0, 1, vDT::eCombinedImageSampler, &NormalTexture->GetDescImageInfo()},
      {MaterialDescriptorSet.Get(), 2, 0, 1, vDT::eCombinedImageSampler, &AOTexture->GetDescImageInfo()},
      {MaterialDescriptorSet.Get(), 3, 0, 1, vDT::eCombinedImageSampler, &MetallicTexture->GetDescImageInfo()},
      {MaterialDescriptorSet.Get(), 4, 0, 1, vDT::eCombinedImageSampler, &RoughnessTexture->GetDescImageInfo()},
    };
    MaterialDescriptorSet.Update(true);
  }
}
