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
  enum class AlphaMode : uint32_t {
    Opaque = 0,
    Blend,
    Mask,
  };

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
    float alpha_cutoff = 0.5f;
    GLSL_BOOL double_sided = false;
    uint32_t uv_scale = 1;
    uint32_t alpha_mode = (uint32_t)AlphaMode::Opaque;
    uint32_t _pad;
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

  void create(const std::string& material_name = "Material");
  void bind_textures(vuk::CommandBuffer& command_buffer) const;
  void reset();
  void destroy();

  bool is_opaque() const;
  const char* alpha_mode_to_string() const;
};
}
