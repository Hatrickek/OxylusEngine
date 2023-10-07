#pragma once

#include <memory>
#include <vector>
#include <vuk/Image.hpp>

namespace vuk {
inline SamplerCreateInfo LinearSamplerRepeated = {
  .magFilter = Filter::eLinear,
  .minFilter = Filter::eLinear,
  .mipmapMode = SamplerMipmapMode::eLinear,
  .addressModeU = SamplerAddressMode::eRepeat,
  .addressModeV = SamplerAddressMode::eRepeat,
  .addressModeW = SamplerAddressMode::eRepeat
};

inline SamplerCreateInfo LinearSamplerClamped = {
  .magFilter = Filter::eLinear,
  .minFilter = Filter::eLinear,
  .mipmapMode = SamplerMipmapMode::eLinear,
  .addressModeU = SamplerAddressMode::eClampToEdge,
  .addressModeV = SamplerAddressMode::eClampToEdge,
  .addressModeW = SamplerAddressMode::eClampToEdge
};
std::vector<Name> diverge_image_mips(std::shared_ptr<RenderGraph> rg, std::string_view input_name, uint32_t mip_count);
std::vector<Name> diverge_image_layers(std::shared_ptr<RenderGraph> rg, std::string_view input_name, uint32_t layer_count);
void generate_mips(std::shared_ptr<RenderGraph> rg, std::string_view input_name, std::string_view output_name, uint32_t mip_count);
}
