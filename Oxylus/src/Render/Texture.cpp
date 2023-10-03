#include "Texture.h"

#include <filesystem>

#include <stb_image.h>

#include "Utils/Log.h"

namespace Oxylus {
uint8_t* Texture::load_stb_image(const std::string& filename, uint32_t* width, uint32_t* height, uint32_t* bits, bool flipY, bool srgb) {
  const auto filePath = std::filesystem::path(filename);

  if (!exists(filePath))
    OX_CORE_ERROR("Couldn't load image, file doesn't exists. {}", filePath.string());

  int tex_width = 0, tex_height = 0, tex_channels = 0;
  constexpr int size_of_channel = 8;
  const auto pixels = stbi_load(filename.c_str(), &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);

  // Return magenta checkerboad image
  if (!pixels) {
    OX_CORE_ERROR("Could not load image '{0}'!", filename);

    tex_channels = 4;

    if (width)
      *width = 2;
    if (height)
      *height = 2;
    if (bits)
      *bits = tex_channels * size_of_channel;

    return get_magenta_texture(*width, *height, tex_channels);
  }

  // TODO support different texChannels
  if (tex_channels != 4)
    tex_channels = 4;

  if (width)
    *width = tex_width;
  if (height)
    *height = tex_height;
  if (bits)
    *bits = tex_channels * size_of_channel; // texChannels;	  //32 bits for 4 bytes r g b a

  const int32_t size = tex_width * tex_height * tex_channels * size_of_channel / 8;
  auto* result = new uint8_t[size];
  memcpy(result, pixels, size);

  stbi_image_free(pixels);
  return result;
}

uint8_t* Texture::load_stb_image_from_memory(void* buffer, size_t len, uint32_t* width, uint32_t* height, uint32_t* bits, bool flipY, bool srgb) {
  int tex_width = 0, tex_height = 0, tex_channels = 0;
  constexpr int size_of_channel = 8;
  const auto pixels = stbi_load_from_memory((stbi_uc*)buffer, (int)len, &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);

  // Return magenta checkerboad image
  if (!pixels) {
    tex_channels = 4;

    if (width)
      *width = 2;
    if (height)
      *height = 2;
    if (bits)
      *bits = tex_channels * size_of_channel;

    return get_magenta_texture(*width, *height, tex_channels);
  }

  if (tex_channels != 4)
    tex_channels = 4;

  if (width)
    *width = tex_width;
  if (height)
    *height = tex_height;
  if (bits)
    *bits = tex_channels * size_of_channel; // texChannels;	  //32 bits for 4 bytes r g b a

  const int32_t size = tex_width * tex_height * tex_channels * size_of_channel / 8;
  auto* result = new uint8_t[size];
  memcpy(result, pixels, size);

  stbi_image_free(pixels);
  return result;
}

uint8_t* Texture::get_magenta_texture(uint32_t width, uint32_t height, uint32_t channels) {
  const uint32_t size = width * height * channels;
  const auto data = new uint8_t[size];

  const uint8_t magenta[16] = {
    255, 0, 255, 255,
    0, 0, 0, 255,
    0, 0, 0, 255,
    255, 0, 255, 255
  };

  memcpy(data, magenta, size);

  return data;
}
}
