#include "Texture.h"

#include <filesystem>

#include <stb_image.h>

#include "Utils/Log.h"

namespace Ox {
uint8_t* Texture::load_stb_image(const std::string& filename, uint32_t* width, uint32_t* height, uint32_t* bits, bool srgb) {
  const auto filePath = std::filesystem::path(filename);

  if (!exists(filePath))
    OX_CORE_ERROR("Couldn't load image, file doesn't exists. {}", filename);

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

  const uint8_t magenta[16] = {
    255, 0, 255, 255,
    0, 0, 0, 255,
    0, 0, 0, 255,
    255, 0, 255, 255
  };

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
}
