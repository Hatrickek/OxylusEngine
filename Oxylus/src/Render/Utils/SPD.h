#pragma once

#include "vuk/CommandBuffer.hpp"
#include "vuk/RenderGraph.hpp"
#include "vuk/Future.hpp"
#include <vector>
#include <memory>
#include <array>

#include "Core/FileSystem.h"

namespace vuk {
enum class ReductionType : uint32_t {
  Avg = 0,
  Min = 1,
  Max = 2,
};

// Generate all mips of an image using the Single Pass Downsampler
inline Future generate_mips_spd(Context& ctx, Future src, Future dst, ReductionType type = ReductionType::Avg) {
  if (!ctx.is_pipeline_available("spd_pipeline")) {
    PipelineBaseCreateInfo spd_pci = {};
    spd_pci.add_hlsl(ox::FileSystem::read_shader_file("SPD/SPD.hlsl"), ox::FileSystem::get_shader_path("SPD/SPD.hlsl"), HlslShaderStage::eCompute);
    ctx.create_named_pipeline("spd_pipeline", spd_pci);
  }
  std::shared_ptr<RenderGraph> rgp = std::make_shared<RenderGraph>("generate_mips_spd");
  rgp->attach_in("_src", std::move(src));
  rgp->attach_in("dst", std::move(dst));
  rgp->add_pass({
    .name = "SPD_Pass",
    .resources = {
      "dst"_image >> eComputeRW,      // transition target
      "_src"_image >> eComputeSampled, // additional usage
    },
    .execute = [type](CommandBuffer& command_buffer) {
      const auto src_ia = *command_buffer.get_resource_image_attachment("_src");
      const auto extent = src_ia.extent.extent;
      const auto mips = src_ia.level_count;
      OX_CORE_ASSERT(mips <= 13)
      std::array<ImageAttachment, 13> mip_ia{};
      for (uint32_t i = 0; i < mips; ++i) {
        auto ia = src_ia;
        ia.base_level = i;
        ia.level_count = 1;
        mip_ia[i] = ia;
      }
      Extent2D dispatch;
      dispatch.width = (extent.width + 63) >> 6;
      dispatch.height = (extent.height + 63) >> 6;

      // Bind source mip
      command_buffer.image_barrier("dst", eComputeRW, eComputeSampled, 0, 1); // Prepare initial mip for read
      command_buffer.bind_compute_pipeline("spd_pipeline");
      command_buffer.bind_image(0, 0, mip_ia[0], ImageLayout::eGeneral);
      switch (type) {
        case ReductionType::Avg: {
          command_buffer.bind_sampler(0, 0, {
                                        .minFilter = vuk::Filter::eLinear,
                                        .addressModeU = vuk::SamplerAddressMode::eClampToEdge,
                                        .addressModeV = vuk::SamplerAddressMode::eClampToEdge,
                                      });
          break;
        }
        case ReductionType::Min: {
          static const auto MIN_CLAMP_RMCI = VkSamplerReductionModeCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO,
            .reductionMode = VK_SAMPLER_REDUCTION_MODE_MIN,
          };
          command_buffer.bind_sampler(0, 0, {
                                        .pNext = &MIN_CLAMP_RMCI,
                                        .minFilter = vuk::Filter::eLinear,
                                        .addressModeU = vuk::SamplerAddressMode::eClampToEdge,
                                        .addressModeV = vuk::SamplerAddressMode::eClampToEdge,
                                      });
          break;
        }
        case ReductionType::Max: {
          static const auto MAX_CLAMP_RMCI = VkSamplerReductionModeCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO,
            .reductionMode = VK_SAMPLER_REDUCTION_MODE_MAX,
          };
          command_buffer.bind_sampler(0, 0, {
                                        .pNext = &MAX_CLAMP_RMCI,
                                        .minFilter = vuk::Filter::eLinear,
                                        .addressModeU = vuk::SamplerAddressMode::eClampToEdge,
                                        .addressModeV = vuk::SamplerAddressMode::eClampToEdge,
                                      });
          break;
        }
      }
      *command_buffer.map_scratch_buffer<uint32_t>(0, 1) = 0;
      // Bind target mips
      for (uint32_t i = 1; i < 13; i++)
        command_buffer.bind_image(0, i + 1, mip_ia[std::min(i, mips - 1)], ImageLayout::eGeneral);

      command_buffer.specialize_constants(0, mips - 1);
      command_buffer.specialize_constants(1, dispatch.width * dispatch.height);
      command_buffer.specialize_constants(2, extent.width);
      command_buffer.specialize_constants(3, extent.height);
      command_buffer.specialize_constants(4, extent.width == extent.height && (extent.width & (extent.width - 1)) == 0 ? 1u : 0u);
      command_buffer.specialize_constants(5, static_cast<uint32_t>(type));
      command_buffer.specialize_constants(6, is_format_srgb(src_ia.format) ? 1u : 0u);

      command_buffer.dispatch(dispatch.width, dispatch.height);
      command_buffer.image_barrier("_src", eComputeSampled, eComputeRW, 0, 1); // Reconverge the image
    }
  });

  return {std::move(rgp), "dst+"};
}
}
