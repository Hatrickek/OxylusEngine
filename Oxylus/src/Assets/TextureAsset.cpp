#include "TextureAsset.h"

#include <vuk/Partials.hpp>

#include "Render/RendererCommon.h"
#include "Render/Texture.h"
#include "Render/Vulkan/VulkanContext.h"

#include "Utils/FileSystem.h"
#include "Utils/Profiler.h"

namespace Oxylus {
Shared<TextureAsset> TextureAsset::s_white_texture = nullptr;

TextureAsset::TextureAsset(const std::string& file_path) {
  load(file_path);
}

TextureAsset::TextureAsset(const TextureLoadInfo& info) {
  if (!info.path.empty())
    load(info.path, info.format, info.generate_cubemap_from_hdr, info.generate_mips);
  else
    create_texture(info.width, info.height, info.data, info.format);
}

TextureAsset::~TextureAsset() = default;

void TextureAsset::create_texture(const uint32_t x, const uint32_t y, void* data, const vuk::Format format, bool generate_mips) {
  OX_SCOPED_ZONE;
  auto [tex, tex_fut] = vuk::create_texture(*VulkanContext::get()->superframe_allocator, format, vuk::Extent3D{x, y, 1u}, data, generate_mips);
  texture = std::move(tex);

  vuk::Compiler compiler;
  tex_fut.wait(*VulkanContext::get()->superframe_allocator, compiler);
}

void TextureAsset::load(const std::string& file_path, const vuk::Format format, const bool generate_cubemap_from_hdr, bool generate_mips) {
  path = file_path;

  uint32_t x, y, chans;
  uint8_t* data = Texture::load_stb_image(path, &x, &y, &chans);

  create_texture(x, y, data, format, generate_mips);

  if (FileSystem::get_file_extension(path) == "hdr" && generate_cubemap_from_hdr) {
    auto [image, future] = RendererCommon::generate_cubemap_from_equirectangular(texture);
    vuk::Compiler compiler;
    future.wait(*VulkanContext::get()->superframe_allocator, compiler);

    vuk::ImageViewCreateInfo ivci;
    ivci.format = vuk::Format::eR32G32B32A32Sfloat;
    ivci.image = image->image;
    ivci.subresourceRange.aspectMask = vuk::format_to_aspect(vuk::Format::eR32G32B32A32Sfloat);
    ivci.subresourceRange.baseArrayLayer = 0;
    ivci.subresourceRange.baseMipLevel = 0;
    ivci.subresourceRange.layerCount = 6;
    ivci.subresourceRange.levelCount = 1;
    ivci.viewType = vuk::ImageViewType::eCube;
    texture.view = *vuk::allocate_image_view(*VulkanContext::get()->superframe_allocator, ivci);

    texture.format = vuk::Format::eR32G32B32A32Sfloat;
    texture.extent = vuk::Dimension3D::absolute(2048, 2048, 1).extent;
    texture.image = std::move(image);
    texture.layer_count = 6;
    texture.level_count = 1;
  }

  delete[] data;
}

void TextureAsset::load_from_memory(void* initial_data, const size_t size) {
  uint32_t x, y, chans;
  const auto data = Texture::load_stb_image_from_memory(initial_data, size, &x, &y, &chans);

  create_texture(x, y, data);

  delete[] data;
}

vuk::ImageAttachment TextureAsset::as_attachment() const {
  return vuk::ImageAttachment::from_texture(texture);
}

void TextureAsset::create_white_texture() {
  OX_SCOPED_ZONE;
  s_white_texture = create_shared<TextureAsset>();
  char white_texture_data[16 * 16 * 4];
  memset(white_texture_data, 0xff, 16 * 16 * 4);
  s_white_texture->create_texture(16, 16, white_texture_data, vuk::Format::eR8G8B8A8Unorm, false);
}
}
