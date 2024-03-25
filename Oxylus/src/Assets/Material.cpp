#include "Material.hpp"

#include <vuk/CommandBuffer.hpp>

#include "Render/Utils/VukCommon.hpp"
#include "Render/Vulkan/VkContext.hpp"

#include "Utils/Profiler.hpp"

namespace ox {
Material::Material(const std::string& material_name) {
  create(material_name);
}

Material::~Material() = default;

void Material::create(const std::string& material_name) {
  OX_SCOPED_ZONE;
  name = material_name;
  reset();
}

void Material::bind_textures(vuk::CommandBuffer& command_buffer) const {
  OX_SCOPED_ZONE;
  command_buffer.bind_sampler(1, 0, vuk::LinearSamplerRepeated)
                .bind_sampler(1, 1, vuk::LinearSamplerRepeated)
                .bind_sampler(1, 2, vuk::LinearSamplerRepeated)
                .bind_sampler(1, 3, vuk::LinearSamplerRepeated)
                .bind_sampler(1, 4, vuk::LinearSamplerRepeated);

  command_buffer.bind_image(1, 0, *albedo_texture->get_texture().view)
                .bind_image(1, 1, *normal_texture->get_texture().view)
                .bind_image(1, 2, *ao_texture->get_texture().view)
                .bind_image(1, 3, *physical_texture->get_texture().view)
                .bind_image(1, 4, *emissive_texture->get_texture().view);
}

#define SET_TEXTURE_ID(texture, parameter) \
  if (!texture) \
    parameter = Asset::INVALID_ID; \
  else \
    parameter = texture->get_id();

Material* Material::set_albedo_texture(const Shared<TextureAsset>& texture) {
  albedo_texture = texture;
  SET_TEXTURE_ID(texture, parameters.albedo_map_id)
  return this;
}

Material* Material::set_normal_texture(const Shared<TextureAsset>& texture) {
  normal_texture = texture;
  SET_TEXTURE_ID(texture, parameters.normal_map_id)
  return this;
}

Material* Material::set_physical_texture(const Shared<TextureAsset>& texture) {
  physical_texture = texture;
  SET_TEXTURE_ID(texture, parameters.physical_map_id)
  return this;
}

Material* Material::set_ao_texture(const Shared<TextureAsset>& texture) {
  ao_texture = texture;
  SET_TEXTURE_ID(texture, parameters.ao_map_id)
  return this;
}

Material* Material::set_emissive_texture(const Shared<TextureAsset>& texture) {
  emissive_texture = texture;
  SET_TEXTURE_ID(texture, parameters.emissive_map_id)
  return this;
}

Material* Material::set_color(Vec4 color) {
  parameters.color = color;
  return this;
}

Material* Material::set_roughness(float roughness) {
  parameters.roughness = roughness;
  return this;
}

Material* Material::set_metallic(float metallic) {
  parameters.metallic = metallic;
  return this;
}

Material* Material::set_reflectance(float reflectance) {
  parameters.reflectance = reflectance;
  return this;
}

Material* Material::set_emissive(Vec4 emissive) {
  parameters.emissive = emissive;
  return this;
}

Material* Material::set_alpha_mode(AlphaMode alpha_mode) {
  parameters.alpha_mode = (uint32_t)alpha_mode;
  return this;
}

Material* Material::set_alpha_cutoff(float cutoff) {
  parameters.alpha_cutoff = cutoff;
  return this;
}

Material* Material::set_double_sided(bool double_sided) {
  parameters.double_sided = double_sided;
  return this;
}

bool Material::is_opaque() const {
  return parameters.alpha_mode == (uint32_t)AlphaMode::Opaque;
}

const char* Material::alpha_mode_to_string() const {
  switch ((AlphaMode)parameters.alpha_mode) {
    case AlphaMode::Opaque: return "Opaque";
    case AlphaMode::Mask: return "Mask";
    case AlphaMode::Blend: return "Blend";
    default: return "Unknown";
  }
}

bool Material::operator==(const Material& other) const {
  if (parameters.color == other.parameters.color &&
      parameters.emissive == other.parameters.emissive &&
      parameters.roughness == other.parameters.roughness &&
      parameters.metallic == other.parameters.metallic &&
      parameters.reflectance == other.parameters.reflectance &&
      parameters.normal == other.parameters.normal &&
      parameters.ao == other.parameters.ao &&
      parameters.albedo_map_id == other.parameters.albedo_map_id &&
      parameters.physical_map_id == other.parameters.physical_map_id &&
      parameters.normal_map_id == other.parameters.normal_map_id &&
      parameters.ao_map_id == other.parameters.ao_map_id &&
      parameters.emissive_map_id == other.parameters.emissive_map_id &&
      parameters.alpha_cutoff == other.parameters.alpha_cutoff &&
      parameters.double_sided == other.parameters.double_sided &&
      parameters.uv_scale == other.parameters.uv_scale &&
      parameters.alpha_mode == other.parameters.alpha_mode) {
    return true;
  }

  return false;
}

void Material::reset() {
  albedo_texture = nullptr;
  parameters.albedo_map_id = Asset::INVALID_ID;
  normal_texture = nullptr;
  parameters.normal_map_id = Asset::INVALID_ID;
  ao_texture = nullptr;
  parameters.ao_map_id = Asset::INVALID_ID;
  physical_texture = nullptr;
  parameters.physical_map_id = Asset::INVALID_ID;
  emissive_texture = nullptr;
  parameters.emissive_map_id = Asset::INVALID_ID;
}

void Material::destroy() {
  parameters = {};
}
}
