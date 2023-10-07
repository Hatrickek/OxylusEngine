#pragma once
#include <cstdint>
#include <string>

namespace vuk {
struct Texture;
}

namespace Oxylus {
class Texture {
public:
  /// Loads the given file using stb. Returned data must be freed manually.
  static uint8_t* load_stb_image(const std::string& filename,
                                 uint32_t* width = nullptr,
                                 uint32_t* height = nullptr,
                                 uint32_t* bits = nullptr,
                                 bool srgb = true);
  static uint8_t* load_stb_image_from_memory(void* buffer,
                                         size_t len,
                                         uint32_t* width = nullptr,
                                         uint32_t* height = nullptr,
                                         uint32_t* bits = nullptr,
                                         bool flipY = false,
                                         bool srgb = true);

  static uint8_t* get_magenta_texture(uint32_t width, uint32_t height, uint32_t channels);

  static uint8_t* convert_to_four_channels(uint32_t width, uint32_t height, const uint8_t* three_channel_data);
};
}
