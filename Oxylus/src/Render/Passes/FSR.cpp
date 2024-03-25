#include "FSR.hpp"

#include <glm/gtc/packing.hpp>
#include <vuk/Context.hpp>
#include <vuk/Partials.hpp>
#include <vuk/Pipeline.hpp>
#include <vuk/ShaderSource.hpp>

#include "Thread/TaskScheduler.hpp"
#include "Core/App.hpp"
#include "Core/FileSystem.hpp"
#include "Render/Camera.hpp"
#include "Render/Utils/VukCommon.hpp"

#define FFX_CPU
#include "ffx_common_types.h"
#include "ffx_core_cpu.h"
#include "ffx_fsr2_resources.h"

namespace fsr2 {
static constexpr int FFX_FSR2_MAXIMUM_BIAS_TEXTURE_WIDTH = 16;
static constexpr int FFX_FSR2_MAXIMUM_BIAS_TEXTURE_HEIGHT = 16;
#define LOCK_LIFETIME_REMAINING 0
#define LOCK_TEMPORAL_LUMA 1
#define LOCK_TRUST 2

static constexpr float ffxFsr2MaximumBias[] = {
  2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   1.876f, 1.809f, 1.772f, 1.753f, 1.748f, 2.0f,   2.0f,
  2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   1.869f, 1.801f, 1.764f, 1.745f, 1.739f, 2.0f,   2.0f,   2.0f,   2.0f,
  2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   1.976f, 1.841f, 1.774f, 1.737f, 1.716f, 1.71f,  2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,
  2.0f,   2.0f,   2.0f,   2.0f,   1.914f, 1.784f, 1.716f, 1.673f, 1.649f, 1.641f, 2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,
  2.0f,   2.0f,   1.793f, 1.676f, 1.604f, 1.562f, 1.54f,  1.533f, 2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   1.802f,
  1.619f, 1.536f, 1.492f, 1.467f, 1.454f, 1.449f, 2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   1.812f, 1.575f, 1.496f, 1.456f,
  1.432f, 1.416f, 1.408f, 1.405f, 2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   1.555f, 1.479f, 1.438f, 1.413f, 1.398f, 1.387f,
  1.381f, 1.379f, 2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   1.812f, 1.555f, 1.474f, 1.43f,  1.404f, 1.387f, 1.376f, 1.368f, 1.363f, 1.362f,
  2.0f,   2.0f,   2.0f,   2.0f,   2.0f,   1.802f, 1.575f, 1.479f, 1.43f,  1.401f, 1.382f, 1.369f, 1.36f,  1.354f, 1.351f, 1.35f,  2.0f,   2.0f,
  1.976f, 1.914f, 1.793f, 1.619f, 1.496f, 1.438f, 1.404f, 1.382f, 1.367f, 1.357f, 1.349f, 1.344f, 1.341f, 1.34f,  1.876f, 1.869f, 1.841f, 1.784f,
  1.676f, 1.536f, 1.456f, 1.413f, 1.387f, 1.369f, 1.357f, 1.347f, 1.341f, 1.336f, 1.333f, 1.332f, 1.809f, 1.801f, 1.774f, 1.716f, 1.604f, 1.492f,
  1.432f, 1.398f, 1.376f, 1.36f,  1.349f, 1.341f, 1.335f, 1.33f,  1.328f, 1.327f, 1.772f, 1.764f, 1.737f, 1.673f, 1.562f, 1.467f, 1.416f, 1.387f,
  1.368f, 1.354f, 1.344f, 1.336f, 1.33f,  1.326f, 1.323f, 1.323f, 1.753f, 1.745f, 1.716f, 1.649f, 1.54f,  1.454f, 1.408f, 1.381f, 1.363f, 1.351f,
  1.341f, 1.333f, 1.328f, 1.323f, 1.321f, 1.32f,  1.748f, 1.739f, 1.71f,  1.641f, 1.533f, 1.449f, 1.405f, 1.379f, 1.362f, 1.35f,  1.34f,  1.332f,
  1.327f, 1.323f, 1.32f,  1.319f,
};

int32_t ffxFsr2GetJitterPhaseCount(int32_t render_width, int32_t display_width) {
  constexpr float base_phase_count = 8.0f;
  const int32_t jitter_phase_count = int32_t(base_phase_count * pow((float(display_width) / render_width), 2.0f));
  return jitter_phase_count;
}
static float halton(int32_t index, int32_t base) {
  float f = 1.0f, result = 0.0f;

  for (int32_t current_index = index; current_index > 0;) {

    f /= (float)base;
    result = result + f * (float)(current_index % base);
    current_index = (uint32_t)(floorf((float)(current_index) / (float)(base)));
  }

  return result;
}

const float FFX_PI = 3.141592653589793f;
const float FFX_EPSILON = 1e-06f;
static float lanczos2(float value) {
  return abs(value) < FFX_EPSILON ? 1.f : (sinf(FFX_PI * value) / (FFX_PI * value)) * (sinf(0.5f * FFX_PI * value) / (0.5f * FFX_PI * value));
}

void SpdSetup(FfxUInt32x2 dispatchThreadGroupCountXY,         // CPU side: dispatch thread group count xy
              FfxUInt32x2 workGroupOffset,                    // GPU side: pass in as constant
              FfxUInt32x2 numWorkGroupsAndMips,               // GPU side: pass in as constant
              FfxUInt32x4 rectInfo,                           // left, top, width, height
              FfxInt32 mips)                                  // optional: if -1, calculate based on rect width and height
{
  workGroupOffset[0] = rectInfo[0] / 64;                      // rectInfo[0] = left
  workGroupOffset[1] = rectInfo[1] / 64;                      // rectInfo[1] = top

  FfxUInt32 endIndexX = (rectInfo[0] + rectInfo[2] - 1) / 64; // rectInfo[0] = left, rectInfo[2] = width
  FfxUInt32 endIndexY = (rectInfo[1] + rectInfo[3] - 1) / 64; // rectInfo[1] = top, rectInfo[3] = height

  dispatchThreadGroupCountXY[0] = endIndexX + 1 - workGroupOffset[0];
  dispatchThreadGroupCountXY[1] = endIndexY + 1 - workGroupOffset[1];

  numWorkGroupsAndMips[0] = (dispatchThreadGroupCountXY[0]) * (dispatchThreadGroupCountXY[1]);

  if (mips >= 0) {
    numWorkGroupsAndMips[1] = FfxUInt32(mips);
  } else {
    // calculate based on rect width and height
    FfxUInt32 resolution = ffxMax(rectInfo[2], rectInfo[3]);
    numWorkGroupsAndMips[1] = FfxUInt32((ffxMin(floor(log2(FfxFloat32(resolution))), FfxFloat32(12))));
  }
}

void SpdSetup(FfxUInt32x2 dispatchThreadGroupCountXY, // CPU side: dispatch thread group count xy
              FfxUInt32x2 workGroupOffset,            // GPU side: pass in as constant
              FfxUInt32x2 numWorkGroupsAndMips,       // GPU side: pass in as constant
              FfxUInt32x4 rectInfo)                   // left, top, width, height
{
  SpdSetup(dispatchThreadGroupCountXY, workGroupOffset, numWorkGroupsAndMips, rectInfo, -1);
}

void FsrRcasCon(FfxUInt32x4 con,
                // The scale is {0.0 := maximum, to N>0, where N is the number of stops (halving) of the reduction of sharpness}.
                FfxFloat32 sharpness) {
  // Transform from stops to linear value.
  sharpness = exp2(-sharpness);
  FfxFloat32x2 hSharp = {sharpness, sharpness};
  con[0] = ffxAsUInt32(sharpness);
  con[1] = packHalf2x16(hSharp);
  con[2] = 0;
  con[3] = 0;
}
} // namespace fsr2

namespace ox {
Vec2 FSR::get_jitter() const {
  const int32_t phaseCount = fsr2::ffxFsr2GetJitterPhaseCount(fsr2_constants.renderSize[0], fsr2_constants.displaySize[0]);
  float x = fsr2::halton(fsr2_constants.frameIndex % phaseCount + 1, 2) - 0.5f;
  float y = fsr2::halton(fsr2_constants.frameIndex % phaseCount + 1, 3) - 0.5f;
  x = 2 * x / (float)fsr2_constants.renderSize[0];
  y = -2 * y / (float)fsr2_constants.renderSize[1];
  return {x, y};
}
void FSR::create_fs2_resources(vuk::Allocator& allocator, UVec2 render_resolution, UVec2 presentation_resolution) {
  using namespace fsr2;
  fsr2_constants = {};

  fsr2_constants.renderSize[0] = render_resolution.x;
  fsr2_constants.renderSize[1] = render_resolution.y;
  fsr2_constants.displaySize[0] = presentation_resolution.x;
  fsr2_constants.displaySize[1] = presentation_resolution.y;
  fsr2_constants.displaySizeRcp[0] = 1.0f / presentation_resolution.x;
  fsr2_constants.displaySizeRcp[1] = 1.0f / presentation_resolution.y;

  adjusted_color = create_texture(allocator,
                                  vuk::Extent3D{render_resolution.x, render_resolution.y, 1},
                                  vuk::Format::eR16G16B16A16Unorm,
                                  DEFAULT_USAGE_FLAGS | vuk::ImageUsageFlagBits::eStorage);

  exposure = create_texture(allocator, vuk::Extent3D{1, 1, 1}, vuk::Format::eR32G32Sfloat, DEFAULT_USAGE_FLAGS | vuk::ImageUsageFlagBits::eStorage);

  luminance_current = create_texture(allocator,
                                     vuk::Extent3D{render_resolution.x / 2, render_resolution.y / 2, 1},
                                     vuk::Format::eR16Sfloat,
                                     DEFAULT_USAGE_FLAGS | vuk::ImageUsageFlagBits::eStorage);

  luminance_history = create_texture(allocator,
                                     vuk::Extent3D{render_resolution.x, render_resolution.y, 1},
                                     vuk::Format::eR8G8B8A8Unorm,
                                     DEFAULT_USAGE_FLAGS | vuk::ImageUsageFlagBits::eStorage);

  previous_depth = create_texture(allocator,
                                  vuk::Extent3D{render_resolution.x, render_resolution.y, 1},
                                  vuk::Format::eR32Uint,
                                  DEFAULT_USAGE_FLAGS | vuk::ImageUsageFlagBits::eStorage);

  dilated_depth = create_texture(allocator,
                                 vuk::Extent3D{render_resolution.x, render_resolution.y, 1},
                                 vuk::Format::eR16Sfloat,
                                 DEFAULT_USAGE_FLAGS | vuk::ImageUsageFlagBits::eStorage);

  dilated_motion = create_texture(allocator,
                                  vuk::Extent3D{render_resolution.x, render_resolution.y, 1},
                                  vuk::Format::eR16G16Sfloat,
                                  DEFAULT_USAGE_FLAGS | vuk::ImageUsageFlagBits::eStorage);

  dilated_reactive = create_texture(allocator,
                                    vuk::Extent3D{render_resolution.x, render_resolution.y, 1},
                                    vuk::Format::eR8G8Unorm,
                                    DEFAULT_USAGE_FLAGS | vuk::ImageUsageFlagBits::eStorage);

  disocclusion_mask = create_texture(allocator,
                                     vuk::Extent3D{render_resolution.x, render_resolution.y, 1},
                                     vuk::Format::eR8Unorm,
                                     DEFAULT_USAGE_FLAGS | vuk::ImageUsageFlagBits::eStorage);

  reactive_mask = create_texture(allocator,
                                 vuk::Extent3D{render_resolution.x, render_resolution.y, 1},
                                 vuk::Format::eR8Unorm,
                                 DEFAULT_USAGE_FLAGS | vuk::ImageUsageFlagBits::eStorage);

  lock_status[0] = create_texture(allocator,
                                  vuk::Extent3D{presentation_resolution.x, presentation_resolution.y, 1},
                                  vuk::Format::eB10G11R11UfloatPack32,
                                  DEFAULT_USAGE_FLAGS | vuk::ImageUsageFlagBits::eStorage);

  lock_status[1] = create_texture(allocator,
                                  vuk::Extent3D{presentation_resolution.x, presentation_resolution.y, 1},
                                  vuk::Format::eB10G11R11UfloatPack32,
                                  DEFAULT_USAGE_FLAGS | vuk::ImageUsageFlagBits::eStorage);

  output_internal[0] = create_texture(allocator,
                                      vuk::Extent3D{presentation_resolution.x, presentation_resolution.y, 1},
                                      vuk::Format::eR16G16B16A16Sfloat,
                                      DEFAULT_USAGE_FLAGS | vuk::ImageUsageFlagBits::eStorage);

  output_internal[1] = create_texture(allocator,
                                      vuk::Extent3D{presentation_resolution.x, presentation_resolution.y, 1},
                                      vuk::Format::eR16G16B16A16Sfloat,
                                      DEFAULT_USAGE_FLAGS | vuk::ImageUsageFlagBits::eStorage);

  {
    // generate the data for the LUT.
    const uint32_t lanczos2LutWidth = 128;
    int16_t lanczos2Weights[lanczos2LutWidth] = {};

    for (uint32_t currentLanczosWidthIndex = 0; currentLanczosWidthIndex < lanczos2LutWidth; currentLanczosWidthIndex++) {
      const float x = 2.0f * currentLanczosWidthIndex / float(lanczos2LutWidth - 1);
      const float y = lanczos2(x);
      lanczos2Weights[currentLanczosWidthIndex] = int16_t(roundf(y * 32767.0f));
    }

    int16_t texture_data[lanczos2LutWidth * sizeof(int16_t)];
    std::memcpy(texture_data, lanczos2Weights, lanczos2LutWidth * sizeof(int16_t));

    auto [tex, tex_fut] = vuk::create_texture(allocator, vuk::Format::eR16Snorm, vuk::Extent3D{lanczos2LutWidth, 1, 1u}, texture_data, false);
    lanczos_lut = std::move(tex);

    vuk::Compiler compiler;
    tex_fut.wait(allocator, compiler);
  }

  {
    int16_t maximumBias[FFX_FSR2_MAXIMUM_BIAS_TEXTURE_WIDTH * FFX_FSR2_MAXIMUM_BIAS_TEXTURE_HEIGHT];
    for (uint32_t i = 0; i < FFX_FSR2_MAXIMUM_BIAS_TEXTURE_WIDTH * FFX_FSR2_MAXIMUM_BIAS_TEXTURE_HEIGHT; ++i) {
      maximumBias[i] = int16_t(roundf(ffxFsr2MaximumBias[i] / 2.0f * 32767.0f));
    }

    int16_t texture_data[FFX_FSR2_MAXIMUM_BIAS_TEXTURE_WIDTH * FFX_FSR2_MAXIMUM_BIAS_TEXTURE_HEIGHT * sizeof(int16_t)];
    std::memcpy(texture_data, maximumBias, FFX_FSR2_MAXIMUM_BIAS_TEXTURE_WIDTH * FFX_FSR2_MAXIMUM_BIAS_TEXTURE_HEIGHT * sizeof(int16_t));

    auto [tex, tex_fut] = vuk::create_texture(allocator,
                                              vuk::Format::eR16Snorm,
                                              vuk::Extent3D{FFX_FSR2_MAXIMUM_BIAS_TEXTURE_WIDTH, FFX_FSR2_MAXIMUM_BIAS_TEXTURE_HEIGHT, 1u},
                                              texture_data,
                                              false);
    maximum_bias_lut = std::move(tex);
  }

  spd_global_atomic =
    create_texture(allocator, vuk::Extent3D{1, 1, 1}, vuk::Format::eR32Uint, DEFAULT_USAGE_FLAGS | vuk::ImageUsageFlagBits::eStorage);
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
void FSR::dispatch(const Shared<vuk::RenderGraph>& rg,
                   Camera& camera,
                   std::string_view input_pre_alpha,
                   std::string_view input_post_alpha,
                   std::string_view input_velocity,
                   std::string_view input_depth,
                   std::string_view output,
                   float dt,
                   float sharpness) {
  using namespace fsr2;

  struct Fsr2SpdConstants {
    uint32_t mips;
    uint32_t numwork_groups;
    uint32_t work_group_offset[2];
    uint32_t render_size[2];
  };

  struct Fsr2RcasConstants {
    uint32_t rcas_config[4];
  };

  auto jitter = get_jitter();
  fsr2_constants.jitterOffset[0] = jitter.x /*camera.jitter.x*/ * fsr2_constants.renderSize[0] * 0.5f;
  fsr2_constants.jitterOffset[1] = jitter.y /*camera.jitter.y*/ * fsr2_constants.renderSize[1] * -0.5f;

  // compute the horizontal FOV for the shader from the vertical one.
  const float aspectRatio = (float)fsr2_constants.renderSize[0] / (float)fsr2_constants.renderSize[1];
  const float cameraAngleHorizontal = atan(tan(camera.get_fov() / 2) * aspectRatio) * 2;
  fsr2_constants.tanHalfFOV = tanf(cameraAngleHorizontal * 0.5f);

  // inverted depth
  fsr2_constants.deviceToViewDepth[0] = FLT_EPSILON;
  fsr2_constants.deviceToViewDepth[1] = -1.00000000f;
  fsr2_constants.deviceToViewDepth[2] = 0.100000001f;
  fsr2_constants.deviceToViewDepth[3] = FLT_EPSILON;

  // To be updated if resource is larger than the actual image size
  fsr2_constants.depthClipUVScale[0] = float(fsr2_constants.renderSize[0]) / disocclusion_mask.extent.width;
  fsr2_constants.depthClipUVScale[1] = float(fsr2_constants.renderSize[1]) / disocclusion_mask.extent.height;
  fsr2_constants.postLockStatusUVScale[0] = float(fsr2_constants.displaySize[0]) / lock_status[0].extent.width;
  fsr2_constants.postLockStatusUVScale[1] = float(fsr2_constants.displaySize[1]) / lock_status[0].extent.height;
  fsr2_constants.reactiveMaskDimRcp[0] = 1.0f / float(reactive_mask.extent.width);
  fsr2_constants.reactiveMaskDimRcp[1] = 1.0f / float(reactive_mask.extent.height);
  fsr2_constants.downscaleFactor[0] = float(fsr2_constants.renderSize[0]) / float(fsr2_constants.displaySize[0]);
  fsr2_constants.downscaleFactor[1] = float(fsr2_constants.renderSize[1]) / float(fsr2_constants.displaySize[1]);
  static float preExposure = 0;
  fsr2_constants.preExposure = (preExposure != 0) ? preExposure : 1.0f;

  // motion vector data
  const bool enable_display_resolution_motion_vectors = false;
  const int32_t* motionVectorsTargetSize = enable_display_resolution_motion_vectors ? fsr2_constants.displaySize : fsr2_constants.renderSize;

  fsr2_constants.motionVectorScale[0] = 1;
  fsr2_constants.motionVectorScale[1] = 1;

  // Jitter cancellation is removed from here because it is baked into the velocity buffer already:

  //// compute jitter cancellation
  // const bool jitterCancellation = true;
  // if (jitterCancellation)
  //{
  //	//fsr2_constants.motionVectorJitterCancellation[0] = (res.jitterPrev.x - fsr2_constants.jitterOffset[0]) / motionVectorsTargetSize[0];
  //	//fsr2_constants.motionVectorJitterCancellation[1] = (res.jitterPrev.y - fsr2_constants.jitterOffset[1]) / motionVectorsTargetSize[1];
  //	fsr2_constants.motionVectorJitterCancellation[0] = res.jitterPrev.x - fsr2_constants.jitterOffset[0];
  //	fsr2_constants.motionVectorJitterCancellation[1] = res.jitterPrev.y - fsr2_constants.jitterOffset[1];

  //	res.jitterPrev.x = fsr2_constants.jitterOffset[0];
  //	res.jitterPrev.y = fsr2_constants.jitterOffset[1];
  //}

  // lock data, assuming jitter sequence length computation for now
  const int32_t jitterPhaseCount = ffxFsr2GetJitterPhaseCount(fsr2_constants.renderSize[0], fsr2_constants.displaySize[0]);

  static const float lockInitialLifetime = 1.0f;
  fsr2_constants.lockInitialLifetime = lockInitialLifetime;

  // init on first frame
  const bool reset_accumulation = fsr2_constants.frameIndex == 0;
  if (reset_accumulation || fsr2_constants.jitterPhaseCount == 0) {
    fsr2_constants.jitterPhaseCount = (float)jitterPhaseCount;
  } else {
    const int32_t jitterPhaseCountDelta = (int32_t)(jitterPhaseCount - fsr2_constants.jitterPhaseCount);
    if (jitterPhaseCountDelta > 0) {
      fsr2_constants.jitterPhaseCount++;
    } else if (jitterPhaseCountDelta < 0) {
      fsr2_constants.jitterPhaseCount--;
    }
  }

  const int32_t maxLockFrames = (int32_t)(fsr2_constants.jitterPhaseCount) + 1;
  fsr2_constants.lockTickDelta = lockInitialLifetime / maxLockFrames;

  // convert delta time to seconds and clamp to [0, 1].
  fsr2_constants.deltaTime = std::min(std::max(dt, 0.0f), 1.0f);

  fsr2_constants.frameIndex++;

  // shading change usage of the SPD mip levels.
  fsr2_constants.lumaMipLevelToUse = uint32_t(FFX_FSR2_SHADING_CHANGE_MIP_LEVEL);

  const float mipDiv = float(2 << fsr2_constants.lumaMipLevelToUse);
  fsr2_constants.lumaMipDimensions[0] = uint32_t(fsr2_constants.renderSize[0] / mipDiv);
  fsr2_constants.lumaMipDimensions[1] = uint32_t(fsr2_constants.renderSize[1] / mipDiv);
  fsr2_constants.lumaMipRcp = float(fsr2_constants.lumaMipDimensions[0] * fsr2_constants.lumaMipDimensions[1]) /
                              float(fsr2_constants.renderSize[0] * fsr2_constants.renderSize[1]);

  // reactive mask bias
  constexpr int32_t thread_group_work_region_dim = 8;
  const int32_t dispatchSrcX = (fsr2_constants.renderSize[0] + (thread_group_work_region_dim - 1)) / thread_group_work_region_dim;
  const int32_t dispatchSrcY = (fsr2_constants.renderSize[1] + (thread_group_work_region_dim - 1)) / thread_group_work_region_dim;
  const int32_t dispatchDstX = (fsr2_constants.displaySize[0] + (thread_group_work_region_dim - 1)) / thread_group_work_region_dim;
  const int32_t dispatchDstY = (fsr2_constants.displaySize[1] + (thread_group_work_region_dim - 1)) / thread_group_work_region_dim;

  // Auto exposure
  uint32_t dispatchThreadGroupCountXY[2];
  uint32_t workGroupOffset[2];
  uint32_t numWorkGroupsAndMips[2];
  uint32_t rectInfo[4] = {0, 0, (uint32_t)fsr2_constants.renderSize[0], (uint32_t)fsr2_constants.renderSize[1]};
  SpdSetup(dispatchThreadGroupCountXY, workGroupOffset, numWorkGroupsAndMips, rectInfo); // TODO: should take reference ?

  // downsample
  Fsr2SpdConstants luminancePyramidConstants;
  luminancePyramidConstants.numwork_groups = numWorkGroupsAndMips[0];
  luminancePyramidConstants.mips = numWorkGroupsAndMips[1];
  luminancePyramidConstants.work_group_offset[0] = workGroupOffset[0];
  luminancePyramidConstants.work_group_offset[1] = workGroupOffset[1];
  luminancePyramidConstants.render_size[0] = fsr2_constants.renderSize[0];
  luminancePyramidConstants.render_size[1] = fsr2_constants.renderSize[1];

  // compute the constants.
  Fsr2RcasConstants rcasConsts = {};
  const float sharpenessRemapped = (-2.0f * sharpness) + 2.0f;
  FsrRcasCon(rcasConsts.rcas_config, sharpenessRemapped);

#define ATTACH_IMAGE(var)                                          \
  rg->attach_image(#var, vuk::ImageAttachment::from_texture(var)); \
  vuk::Name OX_COMBINE_MACRO(r_, var) = #var

  ATTACH_IMAGE(adjusted_color);
  ATTACH_IMAGE(luminance_current);
  ATTACH_IMAGE(luminance_history);
  ATTACH_IMAGE(exposure);
  ATTACH_IMAGE(previous_depth);
  ATTACH_IMAGE(dilated_depth);
  ATTACH_IMAGE(dilated_motion);
  ATTACH_IMAGE(dilated_reactive);
  ATTACH_IMAGE(disocclusion_mask);
  ATTACH_IMAGE(reactive_mask);
  rg->attach_image("output_internal0", vuk::ImageAttachment::from_texture(output_internal[0]));
  vuk::Name r_output_internal0 = "output_internal0";
  rg->attach_image("output_internal1", vuk::ImageAttachment::from_texture(output_internal[1]));
  vuk::Name r_output_internal1 = "output_internal1";
  rg->attach_image("lock_status0", vuk::ImageAttachment::from_texture(lock_status[0]));
  vuk::Name r_lock_status0 = "lock_status0";
  rg->attach_image("lock_status1", vuk::ImageAttachment::from_texture(lock_status[1]));
  vuk::Name r_lock_status1 = "lock_status1";

  rg->attach_image("spd_global_atomic", vuk::ImageAttachment::from_texture(spd_global_atomic));

#define CLEAR_IMAGE(var, clear)                                                     \
  rg->clear_image(#var, OX_EXPAND_STRINGIFY(OX_COMBINE_MACRO(var, _clear)), clear); \
  OX_COMBINE_MACRO(r_, var) = OX_EXPAND_STRINGIFY(OX_COMBINE_MACRO(var, _clear))

  if (reset_accumulation) {
    CLEAR_IMAGE(adjusted_color, vuk::Black<float>);
    CLEAR_IMAGE(luminance_current, vuk::Black<float>);
    CLEAR_IMAGE(luminance_history, vuk::Black<float>);
    CLEAR_IMAGE(exposure, vuk::Black<float>);
    CLEAR_IMAGE(dilated_depth, vuk::Black<float>);
    CLEAR_IMAGE(dilated_motion, vuk::Black<float>);
    CLEAR_IMAGE(dilated_reactive, vuk::Black<float>);
    CLEAR_IMAGE(disocclusion_mask, vuk::Black<float>);
    CLEAR_IMAGE(reactive_mask, vuk::Black<float>);
    rg->clear_image("output_internal0", "output_internal0_clear", vuk::Black<float>);
    r_output_internal0 = "output_internal0_clear";
    rg->clear_image("output_internal1", "output_internal1_clear", vuk::Black<float>);
    r_output_internal1 = "output_internal1_clear";

    float clearValuesLockStatus[4]{};
    clearValuesLockStatus[LOCK_LIFETIME_REMAINING] = lockInitialLifetime * 2.0f;
    clearValuesLockStatus[LOCK_TEMPORAL_LUMA] = 0.0f;
    clearValuesLockStatus[LOCK_TRUST] = 1.0f;
    uint32_t clear_lock_pk = glm::packF2x11_1x10(Vec3(clearValuesLockStatus[0], clearValuesLockStatus[1], clearValuesLockStatus[2]));
    rg->clear_image("lock_status0", "lock_status0_clear", vuk::Clear{vuk::ClearColor{clear_lock_pk, clear_lock_pk, clear_lock_pk, clear_lock_pk}});
    r_lock_status0 = "lock_status0_clear";
    rg->clear_image("lock_status1", "lock_status1_clear", vuk::Clear{vuk::ClearColor{clear_lock_pk, clear_lock_pk, clear_lock_pk, clear_lock_pk}});
    r_lock_status1 = "lock_status1_clear";
  }

  const int r_idx = fsr2_constants.frameIndex % 2;
  const int rw_idx = (fsr2_constants.frameIndex + 1) % 2;

  const auto& r_lock = r_idx == 0 ? r_lock_status0 : r_lock_status1;
  const auto& rw_lock = rw_idx == 0 ? r_lock_status0 : r_lock_status1;
  const auto& r_output = r_idx == 0 ? r_output_internal0 : r_output_internal1;
  const auto& rw_output = rw_idx == 0 ? r_output_internal0 : r_output_internal1;

  const std::vector autogen_resources = {
    vuk::Resource{r_reactive_mask, vuk::Resource::Type::eImage, vuk::eComputeRW},
    vuk::Resource{{input_pre_alpha}, vuk::Resource::Type::eImage, vuk::eComputeSampled},
    vuk::Resource{{input_post_alpha}, vuk::Resource::Type::eImage, vuk::eComputeSampled},
  };
  rg->add_pass({.name = "autogen_reactive_mask", .resources = autogen_resources, .execute = [&](vuk::CommandBuffer& command_buffer) {
    command_buffer.bind_compute_pipeline("ffx_fsr2_autogen_reactive_pass")
      .bind_image(0, 0, {input_pre_alpha})
      .bind_image(0, 1, {input_post_alpha})
      .bind_image(0, 0, r_reactive_mask);

    struct Fsr2GenerateReactiveConstants {
      float scale;
      float threshold;
      float binaryValue;
      uint32_t flags;
    };
    static float scale = 1.0f;
    static float threshold = 0.2f;
    static float binaryValue = 0.9f;
    uint32_t flags = 0;
    flags |= FFX_FSR2_AUTOREACTIVEFLAGS_APPLY_TONEMAP;
    // constants.flags |= FFX_FSR2_AUTOREACTIVEFLAGS_APPLY_INVERSETONEMAP;
    // constants.flags |= FFX_FSR2_AUTOREACTIVEFLAGS_USE_COMPONENTS_MAX;
    // constants.flags |= FFX_FSR2_AUTOREACTIVEFLAGS_APPLY_THRESHOLD;
    const Fsr2GenerateReactiveConstants constants = {
      .scale = scale,
      .threshold = threshold,
      .binaryValue = binaryValue,
      .flags = flags,
    };

    auto* buff = command_buffer.map_scratch_buffer<Fsr2GenerateReactiveConstants>(0, 0);
    *buff = constants;

    const int32_t dispatch_x = (fsr2_constants.renderSize[0] + (thread_group_work_region_dim - 1)) / thread_group_work_region_dim;
    const int32_t dispatch_y = (fsr2_constants.renderSize[1] + (thread_group_work_region_dim - 1)) / thread_group_work_region_dim;

    command_buffer.dispatch(dispatch_x, dispatch_y, 1);
  }});

  // device->BindDynamicConstantBuffer(fsr2_constants, 0, cmd);
  // device->BindSampler(&samplers[SAMPLER_POINT_CLAMP], 0, cmd);
  // device->BindSampler(&samplers[SAMPLER_LINEAR_CLAMP], 1, cmd);

  const std::vector luminance_pyramid_resources = {
    vuk::Resource{"spd_global_atomic", vuk::Resource::Type::eImage, vuk::eComputeRW},
    vuk::Resource{r_luminance_current, vuk::Resource::Type::eImage, vuk::eComputeRW},
    vuk::Resource{r_exposure, vuk::Resource::Type::eImage, vuk::eComputeRW},
    vuk::Resource{{input_post_alpha}, vuk::Resource::Type::eImage, vuk::eComputeSampled},
  };

  rg->add_pass({.name = "compute_luminance_pyramid", .resources = luminance_pyramid_resources, .execute = [&](vuk::CommandBuffer& command_buffer) {
    command_buffer.bind_compute_pipeline("compute_luminance_pyramid_pass")
      .bind_image(0, 0, {input_post_alpha})
      .bind_image(0, 0, "spd_global_atomic")
      .bind_image(0, 1, r_luminance_current) // fsr2_constants.lumaMipLevelToUse mip
      .bind_image(0, 2, r_luminance_current) // 5 mip
      .bind_image(0, 3, r_exposure);

    auto* constants = command_buffer.map_scratch_buffer<Fsr2Constants>(0, 0);
    *constants = fsr2_constants;

    auto* spd_constants = command_buffer.map_scratch_buffer<Fsr2SpdConstants>(0, 1);
    *spd_constants = luminancePyramidConstants;

    command_buffer.dispatch(dispatchThreadGroupCountXY[0], dispatchThreadGroupCountXY[1], 1);
  }});

  const std::vector prepare_input_color_resources = {
    vuk::Resource{r_previous_depth, vuk::Resource::Type::eImage, vuk::eComputeRW},
    vuk::Resource{r_luminance_history, vuk::Resource::Type::eImage, vuk::eComputeRW},
    vuk::Resource{r_adjusted_color, vuk::Resource::Type::eImage, vuk::eComputeRW},
    vuk::Resource{r_exposure.append("+"), vuk::Resource::Type::eImage, vuk::eComputeSampled},
    vuk::Resource{{input_post_alpha}, vuk::Resource::Type::eImage, vuk::eComputeSampled},
  };

  rg->add_pass({.name = "prepare_input_color", .resources = prepare_input_color_resources, .execute = [&](vuk::CommandBuffer& command_buffer) {
    command_buffer.bind_compute_pipeline("prepare_input_color_pass")
      .bind_image(0, 0, {input_post_alpha})
      .bind_image(0, 1, r_exposure)
      .bind_image(0, 1, r_previous_depth)
      .bind_image(0, 1, r_adjusted_color)
      .bind_image(0, 3, r_luminance_history)
      .dispatch(dispatchSrcX, dispatchSrcY, 1);
  }});

  const std::vector reconstruct_previous_depth_resources = {
    vuk::Resource{r_previous_depth.append("+"), vuk::Resource::Type::eImage, vuk::eComputeRW},
    vuk::Resource{r_dilated_motion, vuk::Resource::Type::eImage, vuk::eComputeRW},
    vuk::Resource{r_dilated_depth, vuk::Resource::Type::eImage, vuk::eComputeRW},
    vuk::Resource{r_dilated_reactive, vuk::Resource::Type::eImage, vuk::eComputeRW},
    vuk::Resource{r_exposure.append("+"), vuk::Resource::Type::eImage, vuk::eComputeSampled},
    vuk::Resource{r_reactive_mask.append("+"), vuk::Resource::Type::eImage, vuk::eComputeSampled},
    vuk::Resource{r_adjusted_color.append("+"), vuk::Resource::Type::eImage, vuk::eComputeSampled},
    vuk::Resource{{input_velocity}, vuk::Resource::Type::eImage, vuk::eComputeSampled},
    vuk::Resource{{input_depth}, vuk::Resource::Type::eImage, vuk::eComputeSampled},
    vuk::Resource{{input_post_alpha}, vuk::Resource::Type::eImage, vuk::eComputeSampled},
  };

  rg->add_pass(
    {.name = "reconstruct_previous_depth", .resources = reconstruct_previous_depth_resources, .execute = [&](vuk::CommandBuffer& command_buffer) {
    command_buffer.bind_compute_pipeline("reconstruct_previous_depth_pass")
      .bind_image(0, 0, {input_velocity})
      .bind_image(0, 0, {input_depth})
      .bind_image(0, 0, r_reactive_mask.append("+"))
      .bind_image(0, 0, {input_post_alpha})
      .bind_image(0, 0, r_adjusted_color.append("+"))
      .bind_image(0, 1, r_previous_depth.append("+"))
      .bind_image(0, 1, r_dilated_motion)
      .bind_image(0, 1, r_dilated_depth)
      .bind_image(0, 1, r_dilated_reactive)
      .dispatch(dispatchSrcX, dispatchSrcY, 1);
  }});

  const std::vector depth_clip_resources = {
    vuk::Resource{r_disocclusion_mask, vuk::Resource::Type::eImage, vuk::eComputeRW},
    vuk::Resource{r_previous_depth.append("++"), vuk::Resource::Type::eImage, vuk::eComputeSampled},
    vuk::Resource{r_dilated_motion.append("+"), vuk::Resource::Type::eImage, vuk::eComputeSampled},
    vuk::Resource{r_dilated_depth.append("+"), vuk::Resource::Type::eImage, vuk::eComputeSampled},
  };

  rg->add_pass({.name = "depth_clip", .resources = depth_clip_resources, .execute = [&](vuk::CommandBuffer& command_buffer) {
    command_buffer.bind_compute_pipeline("reconstruct_previous_depth_pass")
      .bind_image(0, 0, r_previous_depth.append("++"))
      .bind_image(0, 0, r_dilated_motion.append("+"))
      .bind_image(0, 0, r_dilated_depth.append("+"))
      .bind_image(0, 0, r_disocclusion_mask)
      .dispatch(dispatchSrcX, dispatchSrcY, 1);
  }});

  const std::vector locks_resources = {
    vuk::Resource{rw_lock, vuk::Resource::Type::eImage, vuk::eComputeRW},
    vuk::Resource{r_reactive_mask.append("+"), vuk::Resource::Type::eImage, vuk::eComputeSampled},
    vuk::Resource{r_adjusted_color.append("+"), vuk::Resource::Type::eImage, vuk::eComputeSampled},
    vuk::Resource{r_lock, vuk::Resource::Type::eImage, vuk::eComputeSampled},
  };

  rg->add_pass({.name = "create_locks", .resources = locks_resources, .execute = [&](vuk::CommandBuffer& command_buffer) {
    command_buffer.bind_compute_pipeline("locks_pass")
      .bind_image(0, 0, r_reactive_mask.append("+"))
      .bind_image(0, 0, r_lock)
      .bind_image(0, 0, r_adjusted_color.append("+"))
      .bind_image(0, 0, rw_lock)
      .dispatch(dispatchSrcX, dispatchSrcY, 1);
  }});

  const std::vector reproject_and_accumulate_resources = {
    vuk::Resource{rw_output, vuk::Resource::Type::eImage, vuk::eComputeRW},
    vuk::Resource{rw_lock.append("+"), vuk::Resource::Type::eImage, vuk::eComputeRW},
    vuk::Resource{r_exposure.append("+"), vuk::Resource::Type::eImage, vuk::eComputeSampled},
    vuk::Resource{r_dilated_motion.append("+"), vuk::Resource::Type::eImage, vuk::eComputeSampled},
    vuk::Resource{r_output.append("+"), vuk::Resource::Type::eImage, vuk::eComputeSampled},
    vuk::Resource{r_lock.append("+"), vuk::Resource::Type::eImage, vuk::eComputeSampled},
    vuk::Resource{r_disocclusion_mask.append("+"), vuk::Resource::Type::eImage, vuk::eComputeSampled},
    vuk::Resource{r_adjusted_color.append("+"), vuk::Resource::Type::eImage, vuk::eComputeSampled},
    vuk::Resource{r_luminance_history.append("+"), vuk::Resource::Type::eImage, vuk::eComputeSampled},
    vuk::Resource{r_dilated_reactive.append("+"), vuk::Resource::Type::eImage, vuk::eComputeSampled},
    vuk::Resource{r_luminance_current.append("+"), vuk::Resource::Type::eImage, vuk::eComputeSampled},
  };

  rg->add_pass(
    {.name = "reproject_and_accumulate", .resources = reproject_and_accumulate_resources, .execute = [&](vuk::CommandBuffer& command_buffer) {
    command_buffer.bind_compute_pipeline("accumulate_pass")
      .bind_image(0, 0, r_exposure.append("+"))
      .bind_image(0, 0, r_dilated_motion.append("+"))
      .bind_image(0, 0, r_output.append("+"))
      .bind_image(0, 0, r_lock.append("+"))
      .bind_image(0, 0, r_disocclusion_mask.append("+"))
      .bind_image(0, 0, r_adjusted_color.append("+"))
      .bind_image(0, 0, r_luminance_history.append("+"))
      .bind_image(0, 0, vuk::ImageAttachment::from_texture(lanczos_lut))
      .bind_image(0, 0, vuk::ImageAttachment::from_texture(maximum_bias_lut))
      .bind_image(0, 0, r_dilated_reactive.append("+"))
      .bind_image(0, 0, r_luminance_current.append("+"))
      .bind_image(1, 0, rw_output)
      .bind_image(1, 0, rw_lock.append("+"))
      .dispatch(dispatchDstX, dispatchDstY, 1);
  }});

  const std::vector sharpen_resources = {
    vuk::Resource{rw_output, vuk::Resource::Type::eImage, vuk::eComputeRW},
    vuk::Resource{rw_lock.append("+"), vuk::Resource::Type::eImage, vuk::eComputeRW},
    vuk::Resource{r_exposure.append("+"), vuk::Resource::Type::eImage, vuk::eComputeSampled},
  };

  rg->add_pass({.name = "sharpen(RCAS)", .resources = sharpen_resources, .execute = [&](vuk::CommandBuffer& command_buffer) {
    command_buffer.bind_compute_pipeline("rcas_pass")
      .bind_image(0, 0, r_exposure.append("+"))
      .bind_image(0, 0, rw_output.append("+"))
      .bind_image(0, 0, {output});

    auto* rcas = command_buffer.map_scratch_buffer<Fsr2RcasConstants>(0, 0);
    *rcas = rcasConsts;

    constexpr int32_t thread_group_work_region_dim_rcas = 16;
    const int32_t dispatchX = (fsr2_constants.displaySize[0] + (thread_group_work_region_dim_rcas - 1)) / thread_group_work_region_dim_rcas;
    const int32_t dispatchY = (fsr2_constants.displaySize[1] + (thread_group_work_region_dim_rcas - 1)) / thread_group_work_region_dim_rcas;

    command_buffer.dispatch(dispatchX, dispatchY, 1);
  }});
}
} // namespace ox
