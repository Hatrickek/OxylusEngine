#pragma once

#include <array>
#include <memory>
#include "vuk/CommandBuffer.hpp"
#include "vuk/Future.hpp"

#include "Core/FileSystem.hpp"

namespace vuk {
enum class ReductionType : uint32_t {
  Avg = 0,
  Min = 1,
  Max = 2,
};

// Generate all mips of an image using the Single Pass Downsampler
inline vuk::Value<vuk::ImageAttachment> generate_mips_spd(Context& ctx,
                                                          vuk::Value<vuk::ImageAttachment>& src,
                                                          vuk::Value<vuk::ImageAttachment>& dst,
                                                          ReductionType type = ReductionType::Avg) {
  if (!ctx.is_pipeline_available("spd_pipeline")) {
    PipelineBaseCreateInfo spd_pci = {};
    spd_pci.add_hlsl(ox::FileSystem::read_shader_file("SPD/SPD.hlsl"), ox::FileSystem::get_shader_path("SPD/SPD.hlsl"), HlslShaderStage::eCompute);
    ctx.create_named_pipeline("spd_pipeline", spd_pci);
  }

  auto pass = vuk::make_pass("SPD", [type](vuk::CommandBuffer& command_buffer, VUK_IA(vuk::eComputeRW) output, VUK_IA(vuk::eComputeSampled) input) {
    const auto extent = input->extent;
    const auto mips = input->level_count;
    OX_ASSERT(mips <= 13);
    std::array<vuk::ImageAttachment, 13> mip_ia{};

    for (uint32_t i = 0; i < mips; ++i) {
      mip_ia[i] = input->mip(i);
    }

    Extent2D dispatch;
    dispatch.width = (extent.width + 63) >> 6;
    dispatch.height = (extent.height + 63) >> 6;

    // Bind source mip
    command_buffer.image_barrier(output, eComputeRW, eComputeSampled, 0, 1); // Prepare initial mip for read
    command_buffer.bind_compute_pipeline("spd_pipeline");
    command_buffer.bind_image(0, 0, mip_ia[0]);
    switch (type) {
      case ReductionType::Avg: {
        command_buffer.bind_sampler(0,
                                    0,
                                    {
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
        command_buffer.bind_sampler(0,
                                    0,
                                    {
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
        command_buffer.bind_sampler(0,
                                    0,
                                    {
                                      .pNext = &MAX_CLAMP_RMCI,
                                      .minFilter = vuk::Filter::eLinear,
                                      .addressModeU = vuk::SamplerAddressMode::eClampToEdge,
                                      .addressModeV = vuk::SamplerAddressMode::eClampToEdge,
                                    });
        break;
      }
    }
    *command_buffer.scratch_buffer<uint32_t>(0, 1) = 0;
    // Bind target mips
    for (uint32_t i = 1; i < 13; i++)
      command_buffer.bind_image(0, i + 1, mip_ia[std::min(i, mips - 1)]);

    command_buffer.specialize_constants(0, mips - 1);
    command_buffer.specialize_constants(1, dispatch.width * dispatch.height);
    command_buffer.specialize_constants(2, extent.width);
    command_buffer.specialize_constants(3, extent.height);
    command_buffer.specialize_constants(4, extent.width == extent.height && (extent.width & (extent.width - 1)) == 0 ? 1u : 0u);
    command_buffer.specialize_constants(5, static_cast<uint32_t>(type));
    command_buffer.specialize_constants(6, is_format_srgb(input->format) ? 1u : 0u);

    command_buffer.dispatch(dispatch.width, dispatch.height);
    command_buffer.image_barrier(input, eComputeSampled, eComputeRW, 0, 1); // Reconverge the image
    return output;
  });

  return pass(src, dst);
}
} // namespace vuk
