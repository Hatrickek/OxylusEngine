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
  } alpha_mode = AlphaMode::Opaque;

  struct Parameters {
    Vec4 color = Vec4(1.0f);
    Vec4 emmisive = Vec4(0);
    float roughness = 1.0f;
    float metallic = 0.0f;
    float specular = 0.0f;
    float normal = 1.0f;
    float ao = 1.0f;
    GLSL_BOOL use_albedo = false;
    GLSL_BOOL use_physical_map = false;
    GLSL_BOOL use_normal = false;
    GLSL_BOOL use_ao = false;
    GLSL_BOOL use_emissive = false;
    GLSL_BOOL use_specular = false;
    float alpha_cutoff = 1;
    GLSL_BOOL double_sided = false;
    uint32_t uv_scale = 1;
    Vec2 _pad;
  } parameters;

  std::string name = "Material";
  std::string path{};

  Ref<TextureAsset> albedo_texture = nullptr;
  Ref<TextureAsset> normal_texture = nullptr;
  Ref<TextureAsset> metallic_roughness_texture = nullptr;
  Ref<TextureAsset> ao_texture = nullptr;
  Ref<TextureAsset> emissive_texture = nullptr;
  Ref<TextureAsset> specular_texture = nullptr;

  Material() = default;
  ~Material();

  void create(const std::string& materialName = "Material");
  void bind_textures(vuk::CommandBuffer& commandBuffer) const;
  void reset();
  void destroy();

  bool is_opaque() const;
};
}
