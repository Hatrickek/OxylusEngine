#pragma once
#include <vuk/Image.hpp>

namespace vuk {
inline vuk::SamplerCreateInfo linear_sampler = {
  .magFilter = vuk::Filter::eLinear,
  .minFilter = vuk::Filter::eLinear,
  .mipmapMode = vuk::SamplerMipmapMode::eLinear,
  .addressModeU = vuk::SamplerAddressMode::eClampToEdge,
  .addressModeV = vuk::SamplerAddressMode::eClampToEdge,
  .addressModeW = vuk::SamplerAddressMode::eClampToEdge
};
}
