#pragma once

#include "glm/vec4.hpp"
#include "Core/Base.h"
#include "Render/Vulkan/VulkanDescriptorSet.h"
#include "Render/Vulkan/VulkanImage.h"
#include "Render/Vulkan/VulkanShader.h"

namespace Oxylus {
#define BOOL int32_t //GLSL doesn't have one byte bools

  class Material {
  public:
    enum class AlphaMode {
      Opaque = 0,
      Mask,
      Blend
    } AlphaMode = AlphaMode::Opaque;

    struct Parameters {
      Vec4 Color = Vec4(1.0f);
      float Roughness = 1.0f;
      float Metallic = 0.0f;
      float Specular = 0.1f;
      float Normal = 1.0f;
      float AO = 1.0f;
      BOOL UseAlbedo = false;
      BOOL UseRoughness = false;
      BOOL UseMetallic = false;
      BOOL UseNormal = false;
      BOOL UseAO = false;
      BOOL UseEmissive = false;
      BOOL UseSpecular = false;
      float AlphaCutoff = 1;
      BOOL DoubleSided = false;
    } Parameters;

    std::string Name = "Material";
    std::string Path{};

    static VulkanDescriptorSet s_DescriptorSet;
    VulkanDescriptorSet MaterialDescriptorSet;
    Ref<VulkanShader> Shader = nullptr;
    Ref<VulkanImage> AlbedoTexture = nullptr;
    Ref<VulkanImage> NormalTexture = nullptr;
    Ref<VulkanImage> RoughnessTexture = nullptr;
    Ref<VulkanImage> MetallicTexture = nullptr;
    Ref<VulkanImage> AOTexture = nullptr;
    Ref<VulkanImage> EmissiveTexture = nullptr;
    Ref<VulkanImage> SpecularTexture = nullptr;
    Ref<VulkanImage> DiffuseTexture = nullptr;

    Material() = default;
    ~Material() = default;

    //TODO: Use ShaderID
    void Create(const std::string& name = "Material", const UUID& shaderID = {});
    bool IsOpaque() const;
    const std::vector<MaterialProperty>& GetMaterialProperties() const { return Shader->GetMaterialProperties(); }
    void Update();
  };
}
