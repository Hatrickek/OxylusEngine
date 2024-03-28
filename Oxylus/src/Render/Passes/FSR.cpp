#include "FSR.hpp"

#include <glm/gtc/packing.hpp>
#include <vuk/Context.hpp>
#include <vuk/Partials.hpp>
#include <vuk/Pipeline.hpp>
#include <vuk/ShaderSource.hpp>

#include "Core/App.hpp"
#include "Core/FileSystem.hpp"
#include "Render/Camera.hpp"
#include "Render/Utils/VukCommon.hpp"
#include "Thread/TaskScheduler.hpp"

#include <ffx_fsr2.h>
#include <vk/ffx_fsr2_vk.h>

#include "Render/Vulkan/VkContext.hpp"

namespace ox {
Vec2 FSR::get_jitter(uint32_t frameIndex, uint32_t renderWidth, uint32_t renderHeight, uint32_t windowWidth) const {
  float jitterX = 0;
  float jitterY = 0;
  ffxFsr2GetJitterOffset(&jitterX, &jitterY, (int)frameIndex, ffxFsr2GetJitterPhaseCount((int)renderWidth, (int)windowWidth));
  return {2.0f * jitterX / static_cast<float>(renderWidth), 2.0f * jitterY / static_cast<float>(renderHeight)};
}

void FSR::create_fs2_resources(UVec2 render_resolution, UVec2 presentation_resolution) {
  const auto vkctx = VkContext::get();

  FfxFsr2Interface interface = {};
  size_t scratchBufferSize = ffxFsr2GetScratchMemorySizeVK(vkctx->physical_device);
  scratch_memory = std::make_unique<char[]>(scratchBufferSize);
  ffxFsr2GetInterfaceVK(&interface, scratch_memory.get(), scratchBufferSize, vkctx->physical_device, vkctx->vkb_instance.fp_vkGetDeviceProcAddr);

  FfxFsr2Context context = {};
  const FfxFsr2ContextDescription description = {
    .flags = FFX_FSR2_ENABLE_DEBUG_CHECKING | FFX_FSR2_ENABLE_AUTO_EXPOSURE | FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE | FFX_FSR2_ENABLE_DEPTH_INFINITE |
             FFX_FSR2_ENABLE_DEPTH_INVERTED,
    .maxRenderSize = {render_resolution.x, render_resolution.y},
    .displaySize = {presentation_resolution.x, presentation_resolution.y},
    .callbacks = interface,
    .fpMessage =
      [](FfxFsr2MsgType type, const wchar_t* message) {
    char cstr[256] = {};
    if (wcstombs_s(nullptr, cstr, sizeof(cstr), message, sizeof(cstr)) == 0) {
      cstr[255] = '\0';
      printf("FSR 2 message (type=%d): %s\n", type, cstr);
    }
  },
  };
  ffxFsr2ContextCreate(&context, &description);
}

void FSR::load_pipelines(vuk::Allocator& allocator, vuk::PipelineBaseCreateInfo& pipeline_ci) {
#define SHADER_FILE(path) FileSystem::read_shader_file(path), FileSystem::get_shader_path(path)

  auto* task_scheduler = App::get_system<TaskScheduler>();

  task_scheduler->add_task([=]() mutable {
    pipeline_ci.add_hlsl(SHADER_FILE("FSR2/ffx_fsr2_autogen_reactive_pass.cso.hlsl"), vuk::HlslShaderStage::eCompute);
    TRY(allocator.get_context().create_named_pipeline("autogen_reactive_pass", pipeline_ci))
  });

  task_scheduler->add_task([=]() mutable {
    pipeline_ci.add_hlsl(SHADER_FILE("FSR2/ffx_fsr2_compute_luminance_pyramid_pass.cso.hlsl"), vuk::HlslShaderStage::eCompute);
    TRY(allocator.get_context().create_named_pipeline("uminance_pyramid_pass", pipeline_ci))
  });

  task_scheduler->add_task([=]() mutable {
    pipeline_ci.add_hlsl(SHADER_FILE("FSR2/ffx_fsr2_prepare_input_color_pass.cso.hlsl"), vuk::HlslShaderStage::eCompute);
    TRY(allocator.get_context().create_named_pipeline("prepare_input_color_pass", pipeline_ci))
  });

  task_scheduler->add_task([=]() mutable {
    pipeline_ci.add_hlsl(SHADER_FILE("FSR2/ffx_fsr2_reconstruct_previous_depth_pass.cso.hlsl"), vuk::HlslShaderStage::eCompute);
    TRY(allocator.get_context().create_named_pipeline("reconstruct_previous_depth_pass", pipeline_ci))
  });

  task_scheduler->add_task([=]() mutable {
    pipeline_ci.add_hlsl(SHADER_FILE("FSR2/ffx_fsr2_depth_clip_pass.cso.hlsl"), vuk::HlslShaderStage::eCompute);
    TRY(allocator.get_context().create_named_pipeline("depth_clip_pass", pipeline_ci))
  });

  task_scheduler->add_task([=]() mutable {
    pipeline_ci.add_hlsl(SHADER_FILE("FSR2/ffx_fsr2_lock_pass.cso.hlsl"), vuk::HlslShaderStage::eCompute);
    TRY(allocator.get_context().create_named_pipeline("lock_pass", pipeline_ci))
  });

  task_scheduler->add_task([=]() mutable {
    pipeline_ci.add_hlsl(SHADER_FILE("FSR2/ffx_fsr2_accumulate_pass.cso.hlsl"), vuk::HlslShaderStage::eCompute);
    TRY(allocator.get_context().create_named_pipeline("accumulate_pass", pipeline_ci))
  });

  task_scheduler->add_task([=]() mutable {
    pipeline_ci.add_hlsl(SHADER_FILE("FSR2/ffx_fsr2_rcas_pass.cso.hlsl"), vuk::HlslShaderStage::eCompute);
    TRY(allocator.get_context().create_named_pipeline("rcas_pass", pipeline_ci))
  });

  task_scheduler->wait_for_all();
}
void FSR::dispatch(Camera& camera, float dt, float sharpness) {}
} // namespace ox
