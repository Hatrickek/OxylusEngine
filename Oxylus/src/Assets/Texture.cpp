#include "Texture.hpp"

#include <filesystem>
#include <stb_image.h>
#include <vuk/Partials.hpp>

#include "Core/FileSystem.hpp"
#include "Render/RendererCommon.h"
#include "Render/Vulkan/VkContext.hpp"
#include "Utils/Log.hpp"
#include "Utils/Profiler.hpp"

namespace ox {
Shared<Texture> Texture::s_white_texture = nullptr;

Texture::Texture(const std::string& file_path) { load(file_path); }

Texture::Texture(const TextureLoadInfo& info) {
  if (!info.path.empty())
    load(info.path, info.format, info.generate_cubemap_from_hdr, info.generate_mips);
  else
    create_texture(info.width, info.height, info.data, info.format);
}

Texture::~Texture() = default;

void Texture::create_texture(const vuk::Extent3D extent, vuk::Format format, vuk::ImageAttachment::Preset preset) {
  const auto ia = vuk::ImageAttachment::from_preset(preset, format, extent, vuk::Samples::e1);
  auto image = vuk::allocate_image(*VkContext::get()->superframe_allocator, ia);
  auto view = vuk::allocate_image_view(*VkContext::get()->superframe_allocator, ia);

  this->image = std::move(*image);
  this->view = std::move(*view);
  this->format = ia.format;
  this->extent = ia.extent;
}

void Texture::create_texture(const uint32_t width, const uint32_t height, const void* data, const vuk::Format format, bool generate_mips) {
  OX_SCOPED_ZONE;
  const auto ia = vuk::ImageAttachment::from_preset(vuk::ImageAttachment::Preset::eGeneric2D, format, extent, vuk::Samples::e1);
  auto& alloc = *VkContext::get()->superframe_allocator;
  auto [tex, view, fut] = vuk::create_image_and_view_with_data(alloc, vuk::DomainFlagBits::eTransferQueue, ia, data);
  vuk::Compiler compiler;
  fut.wait(*VkContext::get()->superframe_allocator, compiler);

  // TODO: generate mips

  this->image = std::move(tex);
  this->extent = {width, height, 1};
  this->format = format;
}

void Texture::load(const std::string& file_path, const vuk::Format format, const bool generate_cubemap_from_hdr, bool generate_mips) {
  path = file_path;

  uint32_t x, y, chans;
  uint8_t* data = load_stb_image(path, &x, &y, &chans);

  create_texture(x, y, data, format, generate_mips);

  if (FileSystem::get_file_extension(path) == "hdr" && generate_cubemap_from_hdr) {
    auto fut = RendererCommon::generate_cubemap_from_equirectangular(as_attachment());
    vuk::Compiler compiler;
    auto val = fut.get(*VkContext::get()->superframe_allocator, compiler);

    this->image = *val->image;
    this->view = *val->image_view;
    this->format = val->format;
    this->extent = val->extent;
  }

  delete[] data;
}

void Texture::load_from_memory(void* initial_data, const size_t size) {
  uint32_t x, y, chans;
  const auto data = load_stb_image_from_memory(initial_data, size, &x, &y, &chans);

  create_texture(x, y, data);

  delete[] data;
}

vuk::ImageAttachment Texture::as_attachment() const {
  auto attch = vuk::ImageAttachment::from_preset(vuk::ImageAttachment::Preset::eGeneric2D, format, extent, vuk::Samples::e1);
  attch.image = *image;
  attch.image_view = *view;
  return attch;
}

void Texture::create_white_texture() {
  OX_SCOPED_ZONE;
  s_white_texture = create_shared<Texture>();
  char white_texture_data[16 * 16 * 4];
  memset(white_texture_data, 0xff, 16 * 16 * 4);
  s_white_texture->create_texture(16, 16, white_texture_data, vuk::Format::eR8G8B8A8Unorm, false);
}

uint8_t* Texture::load_stb_image(const std::string& filename, uint32_t* width, uint32_t* height, uint32_t* bits, bool srgb) {
  const auto filePath = std::filesystem::path(filename);

  if (!exists(filePath))
    OX_LOG_ERROR("Couldn't load image, file doesn't exists. {}", filename);

  int tex_width = 0, tex_height = 0, tex_channels = 0;
  constexpr int size_of_channel = 8;

  const auto pixels = stbi_load(filename.c_str(), &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);

  if (tex_channels != 4)
    tex_channels = 4;

  if (width)
    *width = tex_width;
  if (height)
    *height = tex_height;
  if (bits)
    *bits = tex_channels * size_of_channel;

  const int32_t size = tex_width * tex_height * tex_channels * size_of_channel / 8;
  auto* result = new uint8_t[size];
  memcpy(result, pixels, size);
  stbi_image_free(pixels);

  return result;
}

uint8_t* Texture::load_stb_image_from_memory(void* buffer, size_t len, uint32_t* width, uint32_t* height, uint32_t* bits, bool flipY, bool srgb) {
  int tex_width = 0, tex_height = 0, tex_channels = 0;
  int size_of_channel = 8;
  const auto pixels = stbi_load_from_memory((stbi_uc*)buffer, (int)len, &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);

  if (stbi_is_16_bit_from_memory((stbi_uc*)buffer, (int)len)) {
    size_of_channel = 16;
  }

  if (tex_channels != 4)
    tex_channels = 4;

  if (width)
    *width = tex_width;
  if (height)
    *height = tex_height;
  if (bits)
    *bits = tex_channels * size_of_channel;

  const int32_t size = tex_width * tex_height * tex_channels * size_of_channel / 8;
  auto* result = new uint8_t[size];
  memcpy(result, pixels, size);

  stbi_image_free(pixels);
  return result;
}

uint8_t* Texture::get_magenta_texture(uint32_t width, uint32_t height, uint32_t channels) {
  const uint32_t size = width * height * channels;
  const auto data = new uint8_t[size];

  const uint8_t magenta[16] = {255, 0, 255, 255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 0, 255, 255};

  memcpy(data, magenta, size);

  return data;
}

uint8_t* Texture::convert_to_four_channels(uint32_t width, uint32_t height, const uint8_t* three_channel_data) {
  const auto bufferSize = width * height * 4;
  const auto buffer = new uint8_t[bufferSize];
  auto* rgba = buffer;
  const auto* rgb = three_channel_data;
  for (uint32_t i = 0; i < width * height; ++i) {
    for (uint32_t j = 0; j < 3; ++j) {
      rgba[j] = rgb[j];
    }
    rgba += 4;
    rgb += 3;
  }
  return buffer;
}
} // namespace ox
