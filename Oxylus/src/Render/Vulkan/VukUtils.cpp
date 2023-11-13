#include "VukUtils.h"

#include <vector>
#include <vuk/RenderGraph.hpp>
#include <fmt/format.h>
#include <vuk/CommandBuffer.hpp>

namespace vuk {
std::pair<std::vector<Name>, std::vector<Name>> diverge_image_mips(const std::shared_ptr<RenderGraph>& rg, const std::string_view input_name, const uint32_t mip_count) {
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

std::pair<std::vector<Name>, std::vector<Name>> diverge_image_layers(const std::shared_ptr<RenderGraph>& rg, const std::string_view input_name, uint32_t layer_count) {
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
    Name mip_src_name = {
      std::string_view(fmt::format("{}_mip{}{}", input_name, mip_level - 1, mip_level != 1 ? "+" : ""))
    };
    Name mip_dst_name = {std::string_view(fmt::format("{}_mip{}", input_name, mip_level))};
    Resource src_resource(mip_src_name, Resource::Type::eImage, eTransferRead);
    Resource dst_resource(mip_dst_name, Resource::Type::eImage, eTransferWrite, mip_dst_name.append("+"));
    rg->add_pass({
      .name = {std::string_view(fmt::format("{}_mip{}_gen", input_name, mip_level))},
      .execute_on = DomainFlagBits::eGraphicsOnGraphics,
      .resources = {src_resource, dst_resource},
      .execute = [mip_src_name, mip_dst_name, mip_level](CommandBuffer& command_buffer) {
        ImageBlit blit;
        auto src_ia = *command_buffer.get_resource_image_attachment(mip_src_name);
        auto dim = src_ia.extent;
        assert(dim.sizing == vuk::Sizing::eAbsolute);
        auto extent = dim.extent;
        blit.srcSubresource.aspectMask = vuk::format_to_aspect(src_ia.format);
        blit.srcSubresource.baseArrayLayer = src_ia.base_layer;
        blit.srcSubresource.layerCount = src_ia.layer_count;
        blit.srcSubresource.mipLevel = mip_level - 1;
        blit.srcOffsets[0] = Offset3D{0};
        blit.srcOffsets[1] = Offset3D{
          std::max(static_cast<int32_t>(extent.width) >> (mip_level - 1), 1),
          std::max(static_cast<int32_t>(extent.height) >> (mip_level - 1), 1),
          std::max(static_cast<int32_t>(extent.depth) >> (mip_level - 1), 1)
        };
        blit.dstSubresource = blit.srcSubresource;
        blit.dstSubresource.mipLevel = mip_level;
        blit.dstOffsets[0] = Offset3D{0};
        blit.dstOffsets[1] = Offset3D{
          std::max(static_cast<int32_t>(extent.width) >> (mip_level), 1),
          std::max(static_cast<int32_t>(extent.height) >> (mip_level), 1),
          std::max(static_cast<int32_t>(extent.depth) >> (mip_level), 1)
        };
        command_buffer.blit_image(mip_src_name, mip_dst_name, blit, Filter::eLinear);
      }
    });
  }

  rg->converge_image_explicit(diverged_names, output_name);
}
}
