#include "VukCommon.h"

#include <fmt/format.h>
#include <utility>
#include <vector>
#include <vuk/CommandBuffer.hpp>
#include <vuk/RenderGraph.hpp>

#include "Utils/Log.h"

namespace vuk {
Texture create_texture(Allocator& allocator,
                       Extent3D extent,
                       Format format,
                       ImageUsageFlags usage_flags,
                       bool generate_mips,
                       int array_layers,
                       int mip_level) {
  ImageCreateInfo ici;
  ici.format = format;
  ici.extent = extent;
  ici.samples = Samples::e1;
  ici.imageType = ImageType::e2D;
  ici.initialLayout = ImageLayout::eUndefined;
  ici.tiling = ImageTiling::eOptimal;
  ici.usage = usage_flags;
  const uint32_t mip_count = mip_level == -1 ? (uint32_t)log2f((float)std::max(std::max(extent.width, extent.height), extent.depth)) + 1 : mip_level;
  ici.mipLevels = generate_mips ? mip_count : 1;
  ici.arrayLayers = array_layers;

  return allocator.get_context().allocate_texture(allocator, ici);
}

Texture create_texture(Allocator& allocator, const ImageAttachment& attachment) {
  ImageCreateInfo ici;
  ici.format = attachment.format;
  ici.extent = attachment.extent.extent;
  ici.samples = attachment.sample_count.count;
  ici.imageType = attachment.image_type;
  ici.initialLayout = ImageLayout::eUndefined;
  ici.tiling = attachment.tiling;
  ici.usage = attachment.usage;
  ici.mipLevels = attachment.level_count;
  ici.arrayLayers = attachment.layer_count;
  ici.flags = attachment.image_flags;

  return allocator.get_context().allocate_texture(allocator, ici);
}

std::pair<std::vector<Name>, std::vector<Name>>
diverge_image_mips(const std::shared_ptr<RenderGraph>& rg, const std::string_view input_name, const uint32_t mip_count) {
  std::vector<Name> diverged_names;
  std::vector<Name> output_names = {};
  for (uint32_t mip_level = 0; mip_level < mip_count; mip_level++) {
    Name div_name = Name(input_name).append("_mip").append(std::to_string(mip_level));
    diverged_names.push_back(div_name);
    rg->diverge_image(input_name, {.base_level = mip_level, .level_count = 1}, div_name);

    output_names.emplace_back(div_name.append("+"));
  }
  return {diverged_names, output_names};
}

std::pair<std::vector<Name>, std::vector<Name>>
diverge_image_layers(const std::shared_ptr<RenderGraph>& rg, const std::string_view input_name, uint32_t layer_count) {
  std::vector<Name> diverged_names;
  std::vector<Name> output_names = {};
  for (uint32_t layer_level = 0; layer_level < layer_count; layer_level++) {
    Name div_name = Name(input_name).append("_layer").append(std::to_string(layer_level));
    diverged_names.push_back(div_name);
    rg->diverge_image(input_name, {.base_layer = layer_level, .layer_count = 1}, div_name);

    output_names.emplace_back(div_name.append("+"));
  }
  return {diverged_names, output_names};
}

void generate_mips(const std::shared_ptr<RenderGraph>& rg, std::string_view input_name, const std::string_view output_name, uint32_t mip_count) {
  std::vector<Name> diverged_names;
  for (uint32_t mip_level = 0; mip_level < mip_count; mip_level++) {
    Name div_name = {std::string_view(fmt::format("{}_mip{}", input_name, mip_level))};
    if (mip_level != 0)
      diverged_names.push_back(div_name.append("+"));
    else
      diverged_names.push_back(div_name);
    rg->diverge_image(input_name, {.base_level = mip_level, .level_count = 1}, div_name);
  }

  for (uint32_t mip_level = 1; mip_level < mip_count; mip_level++) {
    Name mip_src_name = {std::string_view(fmt::format("{}_mip{}{}", input_name, mip_level - 1, mip_level != 1 ? "+" : ""))};
    Name mip_dst_name = {std::string_view(fmt::format("{}_mip{}", input_name, mip_level))};
    Resource src_resource(mip_src_name, Resource::Type::eImage, eTransferRead);
    Resource dst_resource(mip_dst_name, Resource::Type::eImage, eTransferWrite, mip_dst_name.append("+"));
    rg->add_pass({.name = {std::string_view(fmt::format("{}_mip{}_gen", input_name, mip_level))},
                  .execute_on = DomainFlagBits::eGraphicsOnGraphics,
                  .resources = {src_resource, dst_resource},
                  .execute = [mip_src_name, mip_dst_name, mip_level](CommandBuffer& command_buffer) {
      ImageBlit blit;
      const auto src_ia = *command_buffer.get_resource_image_attachment(mip_src_name);
      const auto dim = src_ia.extent;
      assert(dim.sizing == vuk::Sizing::eAbsolute);
      const auto extent = dim.extent;
      blit.srcSubresource.aspectMask = format_to_aspect(src_ia.format);
      blit.srcSubresource.baseArrayLayer = src_ia.base_layer;
      blit.srcSubresource.layerCount = src_ia.layer_count;
      blit.srcSubresource.mipLevel = mip_level - 1;
      blit.srcOffsets[0] = Offset3D{0};
      blit.srcOffsets[1] = Offset3D{std::max(static_cast<int32_t>(extent.width) >> (mip_level - 1), 1),
                                    std::max(static_cast<int32_t>(extent.height) >> (mip_level - 1), 1),
                                    std::max(static_cast<int32_t>(extent.depth) >> (mip_level - 1), 1)};
      blit.dstSubresource = blit.srcSubresource;
      blit.dstSubresource.mipLevel = mip_level;
      blit.dstOffsets[0] = Offset3D{0};
      blit.dstOffsets[1] = Offset3D{std::max(static_cast<int32_t>(extent.width) >> (mip_level), 1),
                                    std::max(static_cast<int32_t>(extent.height) >> (mip_level), 1),
                                    std::max(static_cast<int32_t>(extent.depth) >> (mip_level), 1)};
      command_buffer.blit_image(mip_src_name, mip_dst_name, blit, Filter::eLinear);
    }});
  }

  rg->converge_image_explicit(diverged_names, output_name);
}

Future blit_image(Future src, Future dst) {
  std::shared_ptr<RenderGraph> rg = std::make_shared<RenderGraph>("blit_image");

  rg->attach_in("src", std::move(src));
  rg->attach_in("dst", std::move(dst));
  const std::vector<Resource> resources = {"src"_image >> eTransferRead, "dst"_image >> eTransferWrite};

  blit_image_impl(rg.get(), resources[0], resources[1]);

  return {std::move(rg), "dst+"};
}

void blit_image(RenderGraph* rg, std::string_view src, std::string_view dst) {
  const Resource sr = Resource(src, Resource::Type::eImage, eTransferRead);
  const Resource dr = Resource(dst, Resource::Type::eImage, eTransferWrite, std::string(dst).append("+").c_str());

  blit_image_impl(rg, sr, dr);
}

void blit_image_impl(RenderGraph* rg, const Resource& src, const Resource& dst) {
  const auto resources = std::vector{src, dst};
  rg->add_pass({.name = "blit_image_pass", .resources = resources, .execute = [src, dst](CommandBuffer& command_buffer) {
    ImageBlit blit;
    auto src_ia = *command_buffer.get_resource_image_attachment(src.name.name);
    auto dst_ia = *command_buffer.get_resource_image_attachment(dst.name.name);
    OX_ASSERT(src_ia.extent.extent == dst_ia.extent.extent);
    auto src_extent = src_ia.extent.extent;
    blit.srcSubresource.aspectMask = format_to_aspect(src_ia.format);
    blit.srcSubresource.baseArrayLayer = src_ia.base_layer;
    blit.srcSubresource.layerCount = src_ia.layer_count;
    blit.srcSubresource.mipLevel = src_ia.base_level;
    blit.srcOffsets[0] = Offset3D{0};
    blit.srcOffsets[1] = Offset3D{(int32_t)src_extent.width, (int32_t)src_extent.height, (int32_t)src_extent.depth};
    blit.dstSubresource = blit.srcSubresource;
    blit.dstSubresource.mipLevel = dst_ia.base_level;
    blit.dstOffsets[0] = Offset3D{0};
    blit.dstOffsets[1] = Offset3D{(int32_t)src_extent.width, (int32_t)src_extent.height, (int32_t)src_extent.depth};
    command_buffer.blit_image(src.name.name, dst.name.name, blit, Filter::eLinear);
  }});
}

void copy_image(RenderGraph* rg, std::string_view src, std::string_view dst) {
  const Resource sr = Resource(src, Resource::Type::eImage, eTransferRead);
  const Resource dr = Resource(dst, Resource::Type::eImage, eTransferWrite, std::string(dst).append("+").c_str());

  copy_image_impl(rg, sr, dr);
}

void copy_image_impl(RenderGraph* rg, const Resource& src, const Resource& dst) {
  const auto resources = std::vector{src, dst};
  rg->add_pass({.name = "copy_image_pass", .resources = resources, .execute = [src, dst](CommandBuffer& command_buffer) {
    ImageCopy copy;
    auto src_ia = *command_buffer.get_resource_image_attachment(src.name.name);
    auto dst_ia = *command_buffer.get_resource_image_attachment(dst.name.name);
    OX_ASSERT(src_ia.extent.extent == dst_ia.extent.extent);
    copy.srcSubresource.aspectMask = format_to_aspect(src_ia.format);
    copy.srcSubresource.baseArrayLayer = src_ia.base_layer;
    copy.srcSubresource.layerCount = src_ia.layer_count;
    copy.srcSubresource.mipLevel = src_ia.base_level;
    copy.srcOffsets = Offset3D{0};
    copy.dstSubresource = copy.srcSubresource;
    copy.dstSubresource.mipLevel = dst_ia.base_level;
    copy.dstOffsets = Offset3D{0};
    copy.imageExtent = src_ia.extent.extent;
    command_buffer.copy_image(src.name.name, dst.name.name, copy);
  }});
}
} // namespace vuk
