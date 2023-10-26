#pragma once

#include <memory>
#include <vector>
#include <vuk/Image.hpp>

namespace vuk {
inline SamplerCreateInfo NearestMagLinearMinSamplerClamped = {
  .magFilter = Filter::eLinear,
  .minFilter = Filter::eNearest,
  .mipmapMode = SamplerMipmapMode::eNearest,
  .addressModeU = SamplerAddressMode::eClampToEdge,
  .addressModeV = SamplerAddressMode::eClampToEdge,
  .addressModeW = SamplerAddressMode::eClampToEdge
};

inline SamplerCreateInfo LinearMipmapNearestSamplerClamped = {
  .magFilter = Filter::eNearest,
  .minFilter = Filter::eNearest,
  .mipmapMode = SamplerMipmapMode::eLinear,
  .addressModeU = SamplerAddressMode::eClampToEdge,
  .addressModeV = SamplerAddressMode::eClampToEdge,
  .addressModeW = SamplerAddressMode::eClampToEdge
};

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

std::pair<std::vector<Name>, std::vector<Name>> diverge_image_mips(const std::shared_ptr<RenderGraph>& rg, std::string_view input_name, uint32_t mip_count);

// input_names are going to be converted into input_names+
void converge_image_mips(const std::shared_ptr<RenderGraph>& rg, const std::vector<Name>& input_names, std::string_view output_name);

std::pair<std::vector<Name>, std::vector<Name>> diverge_image_layers(const std::shared_ptr<RenderGraph>& rg, std::string_view input_name, uint32_t layer_count);

void generate_mips(const std::shared_ptr<RenderGraph>& rg, std::string_view input_name, std::string_view output_name, uint32_t mip_count);
}
