#pragma once

#include <memory>
#include <vector>
#include <span>
#include <vuk/Image.hpp>
#include <vuk/Future.hpp>

namespace vuk {
struct Resource;
struct ImageAttachment;
inline SamplerCreateInfo NearestSamplerClamped = {
  .magFilter = Filter::eNearest,
  .minFilter = Filter::eNearest,
  .mipmapMode = SamplerMipmapMode::eNearest,
  .addressModeU = SamplerAddressMode::eClampToEdge,
  .addressModeV = SamplerAddressMode::eClampToEdge,
  .addressModeW = SamplerAddressMode::eClampToEdge
};


inline SamplerCreateInfo NearestSamplerRepeated = {
  .magFilter = Filter::eNearest,
  .minFilter = Filter::eNearest,
  .mipmapMode = SamplerMipmapMode::eNearest,
  .addressModeU = SamplerAddressMode::eRepeat,
  .addressModeV = SamplerAddressMode::eRepeat,
  .addressModeW = SamplerAddressMode::eRepeat
};

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
  .addressModeW = SamplerAddressMode::eRepeat,
};

inline SamplerCreateInfo LinearSamplerRepeatedAnisotropy = {
  .magFilter = Filter::eLinear,
  .minFilter = Filter::eLinear,
  .mipmapMode = SamplerMipmapMode::eLinear,
  .addressModeU = SamplerAddressMode::eRepeat,
  .addressModeV = SamplerAddressMode::eRepeat,
  .addressModeW = SamplerAddressMode::eRepeat,
  .anisotropyEnable = true,
  .maxAnisotropy = 16.0f
};

inline SamplerCreateInfo LinearSamplerClamped = {
  .magFilter = Filter::eLinear,
  .minFilter = Filter::eLinear,
  .mipmapMode = SamplerMipmapMode::eLinear,
  .addressModeU = SamplerAddressMode::eClampToEdge,
  .addressModeV = SamplerAddressMode::eClampToEdge,
  .addressModeW = SamplerAddressMode::eClampToEdge,
  .borderColor = BorderColor::eFloatOpaqueWhite
};

inline SamplerCreateInfo CmpDepthSampler = {
  .magFilter = Filter::eLinear,
  .minFilter = Filter::eLinear,
  .mipmapMode = SamplerMipmapMode::eNearest,
  .addressModeU = SamplerAddressMode::eClampToEdge,
  .addressModeV = SamplerAddressMode::eClampToEdge,
  .addressModeW = SamplerAddressMode::eClampToEdge,
  .compareEnable = true,
  .compareOp = CompareOp::eGreaterOrEqual,
  .minLod = 0.0f,
  .maxLod = 0.0f,
};

template <class T>
std::pair<Unique<Buffer>, Future> create_cpu_buffer(Allocator& allocator, std::span<T> data) {
  return create_buffer(allocator, MemoryUsage::eCPUtoGPU, DomainFlagBits::eTransferOnGraphics, data);
}

#define DEFAULT_USAGE_FLAGS vuk::ImageUsageFlagBits::eTransferSrc | vuk::ImageUsageFlagBits::eTransferDst | vuk::ImageUsageFlagBits::eSampled

Texture create_texture(Allocator& allocator, Extent3D extent, Format format, ImageUsageFlags usage_flags, bool generate_mips = false, int array_layers = 1);
Texture create_texture(Allocator& allocator, const ImageAttachment& attachment);

std::pair<std::vector<Name>, std::vector<Name>> diverge_image_mips(const std::shared_ptr<RenderGraph>& rg, std::string_view input_name, uint32_t mip_count);
std::pair<std::vector<Name>, std::vector<Name>> diverge_image_layers(const std::shared_ptr<RenderGraph>& rg, std::string_view input_name, uint32_t layer_count);

void generate_mips(const std::shared_ptr<RenderGraph>& rg, std::string_view input_name, std::string_view output_name, uint32_t mip_count = 0);

Future blit_image(Future src, Future dst);
void blit_image(RenderGraph* rg, std::string_view src, std::string_view dst);
void blit_image_impl(RenderGraph* rg, const Resource& src, const Resource& dst);

void copy_image(RenderGraph* rg, std::string_view src, std::string_view dst);
void copy_image_impl(RenderGraph* rg, const Resource& src, const Resource& dst);
}
