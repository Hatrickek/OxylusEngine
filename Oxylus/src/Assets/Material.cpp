#include "Material.h"

#include <vuk/CommandBuffer.hpp>

#include "Render/Vulkan/VukUtils.h"
#include "Render/Vulkan/VulkanContext.h"

#include "Utils/Profiler.h"

namespace Oxylus {
Material::~Material() { }

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
                .bind_image(1, 3, *metallic_roughness_texture->get_texture().view)
                .bind_image(1, 4, *emissive_texture->get_texture().view);
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

void Material::reset() {
  albedo_texture = TextureAsset::get_purple_texture();
  normal_texture = TextureAsset::get_purple_texture();
  ao_texture = TextureAsset::get_purple_texture();
  metallic_roughness_texture = TextureAsset::get_purple_texture();
  emissive_texture = TextureAsset::get_purple_texture();
}

void Material::destroy() {
  parameters = {};
}
}
