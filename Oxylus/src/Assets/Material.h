#pragma once

#include "glm/vec4.hpp"
#include "Core/Base.h"
#include "Render/Vulkan/VulkanDescriptorSet.h"
#include "Render/Vulkan/VulkanImage.h"
#include "Render/Vulkan/VulkanShader.h"

namespace Oxylus {
  class Material {
  public:
    enum class AlphaMode {
      Opaque = 0,
      Mask,
      Blend
    } AlphaMode = AlphaMode::Opaque;

    struct Parameters {
      glm::vec4 Color = glm::vec4(1.0f);
      float Roughness = 1.0f;
      float Metallic = 0.0f;
      float Specular = 0.1f;
      float Normal = 1.0f;
      float AO = 1.0f;
      int UseAlbedo = 0;
      int UseRoughness = 0;
      int UseMetallic = 0;
      int UseNormal = 0;
      int UseAO = 0;
      float AlphaCutoff = 1;
    } Parameters;

    std::string Name = "Material";
    std::string Path{};

    static VulkanDescriptorSet s_DescriptorSet;
    VulkanDescriptorSet MaterialDescriptorSet;
    Ref<VulkanShader> Shader;
    Ref<VulkanImage> AlbedoTexture;
    Ref<VulkanImage> NormalTexture;
    Ref<VulkanImage> RoughnessTexture;
    Ref<VulkanImage> MetallicTexture;
    Ref<VulkanImage> AOTexture;

    Material() = default;
    ~Material() = default;

    //TODO: Use ShaderID
    void Create(const std::string& name = "Material", const UUID& shaderID = {});
    bool IsOpaque() const;
    const std::vector<MaterialProperty>& GetMaterialProperties() const { return Shader->GetMaterialProperties(); }
    void Update();
  };
}
