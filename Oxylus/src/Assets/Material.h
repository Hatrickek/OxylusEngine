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
    float reflectance = 0.0f;
    float normal = 1.0f;
    float ao = 1.0f;
    int use_albedo = false;
    int use_physical_map = false;
    int use_normal = false;
    int use_ao = false;
    int use_emissive = false;
    float alpha_cutoff = 0.5f;
    int double_sided = false;
    uint32_t uv_scale = 1;
    uint32_t alpha_mode = (uint32_t)AlphaMode::Opaque;
    Vec2 _pad;
  } parameters;

  std::string name = "Material";
  std::string path{};

  Ref<TextureAsset> albedo_texture = nullptr;
  Ref<TextureAsset> normal_texture = nullptr;
  Ref<TextureAsset> metallic_roughness_texture = nullptr;
  Ref<TextureAsset> ao_texture = nullptr;
  Ref<TextureAsset> emissive_texture = nullptr;

  static constexpr auto TOTAL_PBR_MATERIAL_TEXTURE_COUNT = 5;
  static constexpr auto ALBEDO_MAP_INDEX = 0;
  static constexpr auto NORMAL_MAP_INDEX = 1;
  static constexpr auto PHYSICAL_MAP_INDEX = 2;
  static constexpr auto AO_MAP_INDEX = 3;
  static constexpr auto EMISSIVE_MAP_INDEX = 4;

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
