#include "TextureAsset.h"

#include <vuk/Partials.hpp>

#include "Render/Texture.h"
#include "Render/Vulkan/VulkanContext.h"
#include "Render/Vulkan/VulkanRenderer.h"
#include "Utils/EmbeddedMissingTexture.h"
#include "Utils/FileSystem.h"

namespace Oxylus {
Ref<TextureAsset> TextureAsset::s_PurpleTexture = nullptr;
Ref<TextureAsset> TextureAsset::s_WhiteTexture = nullptr;

TextureAsset::TextureAsset(const std::string& path) {
  Load(path);
}

TextureAsset::TextureAsset(const TextureLoadInfo& info) {
  if (!info.Path.empty())
    Load(info.Path, info.Format);
  else
    CreateTexture(info.Width, info.Height, info.Data, info.Format);
}

TextureAsset::~TextureAsset() = default;

void TextureAsset::CreateTexture(uint32_t x, uint32_t y, void* data, vuk::Format format) {
  auto [tex, tex_fut] = create_texture(*VulkanContext::Get()->SuperframeAllocator, format, vuk::Extent3D{x, y, 1u}, data, true);
  m_Texture = std::move(tex);

  vuk::Compiler compiler;
  tex_fut.wait(*VulkanContext::Get()->SuperframeAllocator, compiler);
}

void TextureAsset::Load(const std::string& path, vuk::Format format) {
  m_Path = path;

  uint32_t x, y, chans;
  uint8_t* data = Texture::LoadStbImage(path, &x, &y, &chans);

  CreateTexture(x, y, data, format);

  if (FileSystem::GetFileExtension(path) == "hdr") {
    auto [image, future] = VulkanRenderer::GenerateCubemapFromEquirectangular(m_Texture);
    vuk::Compiler compiler;
    future.wait(*VulkanContext::Get()->SuperframeAllocator, compiler);

    vuk::ImageViewCreateInfo ivci;
    ivci.format = vuk::Format::eR32G32B32A32Sfloat;
    ivci.image = image->image;
    ivci.subresourceRange.aspectMask = vuk::format_to_aspect(vuk::Format::eR32G32B32A32Sfloat);
    ivci.subresourceRange.baseArrayLayer = 0;
    ivci.subresourceRange.baseMipLevel = 0;
    ivci.subresourceRange.layerCount = 6;
    ivci.subresourceRange.levelCount = 1;
    ivci.viewType = vuk::ImageViewType::eCube;
    m_Texture.view = *vuk::allocate_image_view(*VulkanContext::Get()->SuperframeAllocator, ivci);

    m_Texture.format = vuk::Format::eR32G32B32A32Sfloat;
    m_Texture.extent = vuk::Dimension3D::absolute(2048, 2048, 1).extent;
    m_Texture.image = std::move(image);
    m_Texture.layer_count = 6;
    m_Texture.level_count = 1;
  }
}

void TextureAsset::LoadFromMemory(void* initialData, size_t size) {
  uint32_t x, y, chans;
  const auto data = Texture::LoadStbImageFromMemory(initialData, size, &x, &y, &chans);

  CreateTexture(x, y, data);

  delete[] data;
}

vuk::ImageAttachment TextureAsset::AsAttachment() const {
  return vuk::ImageAttachment::from_texture(m_Texture);
}

void TextureAsset::CreateBlankTexture() {
  s_PurpleTexture = CreateRef<TextureAsset>();
  s_PurpleTexture->CreateTexture(MissingTextureWidth, MissingTextureHeight, MissingTexture);
}

void TextureAsset::CreateWhiteTexture() {
  s_WhiteTexture = CreateRef<TextureAsset>();
  uint32_t whiteTextureData = 0xffffffff;
  s_WhiteTexture->CreateTexture(16, 16, &whiteTextureData);
}
}
