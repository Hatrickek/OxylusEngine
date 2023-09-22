#pragma once

#include <string>

#include "TextureAsset.h"
#include "glm/vec4.hpp"
#include "Core/Base.h"
#include "Core/Types.h"

namespace vuk {
class CommandBuffer;
struct Texture;
}

namespace Oxylus {
class Material {
public:
  enum class AlphaMode {
    Opaque = 0,
    Mask,
    Blend
  } AlphaMode = AlphaMode::Opaque;

  struct Parameters {
    Vec4 Color = Vec4(1.0f);
    Vec4 Emmisive = Vec4(0);
    float Roughness = 1.0f;
    float Metallic = 0.0f;
    float Specular = 0.0f;
    float Normal = 1.0f;
    float AO = 1.0f;
    GLSL_BOOL UseAlbedo = false;
    GLSL_BOOL UsePhysicalMap = false;
    GLSL_BOOL UseNormal = false;
    GLSL_BOOL UseAO = false;
    GLSL_BOOL UseEmissive = false;
    GLSL_BOOL UseSpecular = false;
    float AlphaCutoff = 1;
    GLSL_BOOL DoubleSided = false;
    uint32_t UVScale = 1;
  } m_Parameters;

  std::string Name = "Material";
  std::string Path{};

  Ref<TextureAsset> AlbedoTexture = nullptr;
  Ref<TextureAsset> NormalTexture = nullptr;
  Ref<TextureAsset> MetallicRoughnessTexture = nullptr;
  Ref<TextureAsset> AOTexture = nullptr;
  Ref<TextureAsset> EmissiveTexture = nullptr;
  Ref<TextureAsset> SpecularTexture = nullptr;

  Material() = default;
  ~Material();

  void Create(const std::string& name = "Material");
  void BindTextures(vuk::CommandBuffer& commandBuffer) const;
  void Reset();
  void Destroy();

  bool IsOpaque() const;
};
}
