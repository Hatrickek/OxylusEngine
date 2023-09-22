#include "Material.h"

#include <vuk/CommandBuffer.hpp>

#include "Render/Vulkan/VulkanRenderer.h"

namespace Oxylus {
Material::~Material() { }

void Material::Create(const std::string& name) {
  OX_SCOPED_ZONE;
  Name = name;
  Reset();
}

void Material::BindTextures(vuk::CommandBuffer& commandBuffer) const {
  commandBuffer.bind_sampler(1, 0, {})
               .bind_sampler(1, 1, {})
               .bind_sampler(1, 2, {})
               .bind_sampler(1, 3, {});

  commandBuffer.bind_image(1, 0, *AlbedoTexture->GetTexture().view)
               .bind_image(1, 1, *NormalTexture->GetTexture().view)
               .bind_image(1, 2, *AOTexture->GetTexture().view)
               .bind_image(1, 3, *MetallicRoughnessTexture->GetTexture().view);
}

bool Material::IsOpaque() const {
  return AlphaMode == AlphaMode::Opaque;
}

void Material::Reset() {
  AlbedoTexture = TextureAsset::GetBlankTexture();
  NormalTexture = TextureAsset::GetBlankTexture();
  AOTexture = TextureAsset::GetBlankTexture();
  MetallicRoughnessTexture = TextureAsset::GetBlankTexture();
}

void Material::Destroy() {
  m_Parameters = {};
}
}
