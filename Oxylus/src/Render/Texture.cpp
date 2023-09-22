#include "Texture.h"

#include <filesystem>

#include <stb_image.h>

#include "Utils/Log.h"

namespace Oxylus {
uint8_t* Texture::LoadStbImage(const std::string& filename, uint32_t* width, uint32_t* height, uint32_t* bits, bool flipY, bool srgb) {
  const auto filePath = std::filesystem::path(filename);

  if (!exists(filePath))
    OX_CORE_ERROR("Couldn't load image, file doesn't exists. {}", filePath.string());

  int texWidth = 0, texHeight = 0, texChannels = 0;
  constexpr int sizeOfChannel = 8;
  const auto pixels = stbi_load(filename.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

  // Return magenta checkerboad image
  if (!pixels) {
    OX_CORE_ERROR("Could not load image '{0}'!", filename);

    texChannels = 4;

    if (width)
      *width = 2;
    if (height)
      *height = 2;
    if (bits)
      *bits = texChannels * sizeOfChannel;

    return GetMagentaTexture(*width, *height, texChannels);
  }

  // TODO support different texChannels
  if (texChannels != 4)
    texChannels = 4;

  if (width)
    *width = texWidth;
  if (height)
    *height = texHeight;
  if (bits)
    *bits = texChannels * sizeOfChannel; // texChannels;	  //32 bits for 4 bytes r g b a

  const int32_t size = texWidth * texHeight * texChannels * sizeOfChannel / 8;
  auto* result = new uint8_t[size];
  memcpy(result, pixels, size);

  stbi_image_free(pixels);
  return result;
}

uint8_t* Texture::LoadStbImageFromMemory(void* buffer, size_t len, uint32_t* width, uint32_t* height, uint32_t* bits, bool flipY, bool srgb) {
  int texWidth = 0, texHeight = 0, texChannels = 0;
  constexpr int sizeOfChannel = 8;
  const auto pixels = stbi_load_from_memory((stbi_uc*)buffer, (int)len, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

  // Return magenta checkerboad image
  if (!pixels) {
    texChannels = 4;

    if (width)
      *width = 2;
    if (height)
      *height = 2;
    if (bits)
      *bits = texChannels * sizeOfChannel;

    return GetMagentaTexture(*width, *height, texChannels);
  }

  if (texChannels != 4)
    texChannels = 4;

  if (width)
    *width = texWidth;
  if (height)
    *height = texHeight;
  if (bits)
    *bits = texChannels * sizeOfChannel; // texChannels;	  //32 bits for 4 bytes r g b a

  const int32_t size = texWidth * texHeight * texChannels * sizeOfChannel / 8;
  auto* result = new uint8_t[size];
  memcpy(result, pixels, size);

  stbi_image_free(pixels);
  return result;
}

uint8_t* Texture::GetMagentaTexture(uint32_t width, uint32_t height, uint32_t channels) {
  const uint32_t size = width * height * channels;
  const auto data = new uint8_t[size];

  const uint8_t datatwo[16] = {
    255, 0, 255, 255,
    0, 0, 0, 255,
    0, 0, 0, 255,
    255, 0, 255, 255
  };

  memcpy(data, datatwo, size);

  return data;
}
}
