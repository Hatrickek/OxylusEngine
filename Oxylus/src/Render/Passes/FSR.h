#pragma once
#include <vuk/Image.hpp>

#include "Core/Base.h"
#include "Core/Types.h"

namespace vuk {
struct PipelineBaseCreateInfo;
class Context;
} // namespace vuk
namespace ox {
class Camera;
class FSR {
public:
  FSR() = default;
  ~FSR() = default;

  struct Fsr2Constants {
    int32_t renderSize[2];
    int32_t displaySize[2];
    uint32_t lumaMipDimensions[2];
    uint32_t lumaMipLevelToUse;
    uint32_t frameIndex;
    float displaySizeRcp[2];
    float jitterOffset[2];
    float deviceToViewDepth[4];
    float depthClipUVScale[2];
    float postLockStatusUVScale[2];
    float reactiveMaskDimRcp[2];
    float motionVectorScale[2];
    float downscaleFactor[2];
    float preExposure;
    float tanHalfFOV;
    float motionVectorJitterCancellation[2];
    float jitterPhaseCount;
    float lockInitialLifetime;
    float lockTickDelta;
    float deltaTime;
    float dynamicResChangeFactor;
    float lumaMipRcp;
  } fsr2_constants = {};

  vuk::Texture adjusted_color;
  vuk::Texture luminance_current;
  vuk::Texture luminance_history;
  vuk::Texture exposure;
  vuk::Texture previous_depth;
  vuk::Texture dilated_depth;
  vuk::Texture dilated_motion;
  vuk::Texture dilated_reactive;
  vuk::Texture disocclusion_mask;
  vuk::Texture lock_status[2];
  vuk::Texture reactive_mask;
  vuk::Texture lanczos_lut;
  vuk::Texture maximum_bias_lut;
  vuk::Texture spd_global_atomic;
  vuk::Texture output_internal[2];

  Vec2 get_jitter() const;

  void create_fs2_resources(vuk::Allocator& allocator, UVec2 render_resolution, UVec2 presentation_resolution);
  void load_pipelines(vuk::Allocator& allocator, vuk::PipelineBaseCreateInfo& pipeline_ci);
  void dispatch(const Shared<vuk::RenderGraph>& rg,
                Camera& camera,
                std::string_view input_pre_alpha,
                std::string_view input_post_alpha,
                std::string_view input_velocity,
                std::string_view input_depth,
                std::string_view output,
                float dt,
                float sharpness);
};
} // namespace ox
