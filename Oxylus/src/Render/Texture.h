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
  static uint8_t* LoadStbImage(const std::string& filename,
                               uint32_t* width = nullptr,
                               uint32_t* height = nullptr,
                               uint32_t* bits = nullptr,
                               bool flipY = false,
                               bool srgb = true);
  static uint8_t* LoadStbImageFromMemory(void* buffer,
                                         size_t len,
                                         uint32_t* width = nullptr,
                                         uint32_t* height = nullptr,
                                         uint32_t* bits = nullptr,
                                         bool flipY = false,
                                         bool srgb = true);

  static uint8_t* GetMagentaTexture(uint32_t width, uint32_t height, uint32_t channels);
};
}
