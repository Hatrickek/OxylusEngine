///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2021, Intel Corporation 
// 
// SPDX-License-Identifier: MIT
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// XeGTAO is based on GTAO/GTSO "Jimenez et al. / Practical Real-Time Strategies for Accurate Indirect Occlusion", 
// https://www.activision.com/cdn/research/Practical_Real_Time_Strategies_for_Accurate_Indirect_Occlusion_NEW%20VERSION_COLOR.pdf
// 
// Implementation:  Filip Strugar (filip.strugar@intel.com), Steve Mccalla <stephen.mccalla@intel.com>         (\_/)
// Version:         1.02                                                                                      (='.'=)
// Details:         https://github.com/GameTechDev/XeGTAO                                                     (")_(")
//
// Version history:
// 1.00 (2021-08-09): Initial release
// 1.01 (2021-09-02): Fix for depth going to inf for 'far' depth buffer values that are out of fp16 range
// 1.02 (2021-09-03): More fast_acos use and made final horizon cos clamping optional (off by default): 3-4% perf boost
// 1.10 (2021-09-03): Added a couple of heuristics to combat over-darkening errors in certain scenarios
// 1.20 (2021-09-06): Optional normal from depth generation is now a standalone pass: no longer integrated into 
//                    main XeGTAO pass to reduce complexity and allow reuse; also quality of generated normals improved
// 1.21 (2021-09-28): Replaced 'groupshared'-based denoiser with a slightly slower multi-pass one where a 2-pass new
//                    equals 1-pass old. However, 1-pass new is faster than the 1-pass old and enough when TAA enabled.
// 1.22 (2021-09-28): Added 'XeGTAO_' prefix to all local functions to avoid name clashes with various user codebases.
// 1.30 (2021-10-10): Added support for directional component (bent normals).
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef __XE_GTAO_TYPES_H__
#define __XE_GTAO_TYPES_H__

#ifdef __cplusplus

namespace XeGTAO {
// cpp<->hlsl mapping
struct Matrix4x4 {
  float m[16];
};

struct Vector3 {
  float x, y, z;
};

struct Vector2 {
  float x, y;
};

struct Vector2i {
  int x, y;
};

using uint = unsigned int;

#else // #ifdef __cplusplus

  // cpp<->hlsl mapping
#define Matrix4x4       float4x4
#define Vector3         float3
#define Vector2         float2
#define Vector2i        int2

#endif

// Global consts that need to be visible from both shader and cpp side
#define XE_GTAO_DEPTH_MIP_LEVELS                    5                   // this one is hard-coded to 5 for now
#define XE_GTAO_NUMTHREADS_X                        8                   // these can be changed
#define XE_GTAO_NUMTHREADS_Y                        8                   // these can be changed

struct GTAOConstants {
  Vector2i ViewportSize;
  Vector2 ViewportPixelSize;                  // .zw == 1.0 / ViewportSize.xy

  Vector2 DepthUnpackConsts;
  Vector2 CameraTanHalfFOV;

  Vector2 NDCToViewMul;
  Vector2 NDCToViewAdd;

  Vector2 NDCToViewMul_x_PixelSize;
  float EffectRadius;                       // world (viewspace) maximum size of the shadow
  float EffectFalloffRange;

  float RadiusMultiplier;
  float Padding0;
  float FinalValuePower;
  float DenoiseBlurBeta;

  float SampleDistributionPower;
  float ThinOccluderCompensation;
  float DepthMIPSamplingOffset;
  int NoiseIndex;                         // frameIndex % 64 if using TAA or 0 otherwise
};

// some constants reduce performance if provided as dynamic values; if these constants are not required to be dynamic and they match default values, 
// set XE_GTAO_USE_DEFAULT_CONSTANTS and the code will compile into a more efficient shader
#define XE_GTAO_DEFAULT_RADIUS_MULTIPLIER               (1.457f  )  // allows us to use different value as compared to ground truth radius to counter inherent screen space biases
#define XE_GTAO_DEFAULT_FALLOFF_RANGE                   (0.615f  )  // distant samples contribute less
#define XE_GTAO_DEFAULT_SAMPLE_DISTRIBUTION_POWER       (2.0f    )  // small crevices more important than big surfaces
#define XE_GTAO_DEFAULT_THIN_OCCLUDER_COMPENSATION      (0.0f    )  // the new 'thickness heuristic' approach
#define XE_GTAO_DEFAULT_FINAL_VALUE_POWER               (2.2f    )  // modifies the final ambient occlusion value using power function - this allows some of the above heuristics to do different things
#define XE_GTAO_DEFAULT_DEPTH_MIP_SAMPLING_OFFSET       (3.30f   )  // main trade-off between performance (memory bandwidth) and quality (temporal stability is the first affected, thin objects next)

#define XE_GTAO_OCCLUSION_TERM_SCALE                    (1.5f)      // for packing in UNORM (because raw, pre-denoised occlusion term can overshoot 1 but will later average out to 1)

// From https://www.shadertoy.com/view/3tB3z3 - except we're using R2 here
#define XE_HILBERT_LEVEL    6U
#define XE_HILBERT_WIDTH    ( (1U << XE_HILBERT_LEVEL) )
#define XE_HILBERT_AREA     ( XE_HILBERT_WIDTH * XE_HILBERT_WIDTH )

inline uint HilbertIndex(uint posX, uint posY) {
  uint index = 0U;
  for (uint curLevel = XE_HILBERT_WIDTH / 2U; curLevel > 0U; curLevel /= 2U) {
    uint regionX = (posX & curLevel) > 0U;
    uint regionY = (posY & curLevel) > 0U;
    index += curLevel * curLevel * ((3U * regionX) ^ regionY);
    if (regionY == 0U) {
      if (regionX == 1U) {
        posX = static_cast<uint>((XE_HILBERT_WIDTH - 1U)) - posX;
        posY = static_cast<uint>((XE_HILBERT_WIDTH - 1U)) - posY;
      }

      uint temp = posX;
      posX = posY;
      posY = temp;
    }
  }
  return index;
}

#ifdef __cplusplus

struct GTAOSettings {
  int QualityLevel = 2;        // 0: low; 1: medium; 2: high; 3: ultra
  int DenoisePasses = 1;        // 0: disabled; 1: sharp; 2: medium; 3: soft
  float Radius = 0.5f;     // [0.0,  ~ ]   World (view) space size of the occlusion sphere.

  // auto-tune-d settings
  float RadiusMultiplier = XE_GTAO_DEFAULT_RADIUS_MULTIPLIER;
  float FalloffRange = XE_GTAO_DEFAULT_FALLOFF_RANGE;
  float SampleDistributionPower = XE_GTAO_DEFAULT_SAMPLE_DISTRIBUTION_POWER;
  float ThinOccluderCompensation = XE_GTAO_DEFAULT_THIN_OCCLUDER_COMPENSATION;
  float FinalValuePower = XE_GTAO_DEFAULT_FINAL_VALUE_POWER;
  float DepthMIPSamplingOffset = XE_GTAO_DEFAULT_DEPTH_MIP_SAMPLING_OFFSET;
};

template <class T>
T clamp(const T& v, const T& min, const T& max) {
  assert(max >= min);
  if (v < min)
    return min;
  if (v > max)
    return max;
  return v;
}

// If using TAA then set noiseIndex to frameIndex % 64 - otherwise use 0
// glm is column major by default
inline void gtao_update_constants(GTAOConstants& consts,
                                int viewportWidth,
                                int viewportHeight,
                                const GTAOSettings& settings,
                                const float projMatrix[16],
                                bool rowMajor,
                                unsigned int frameCounter) {
  consts.ViewportSize = {viewportWidth, viewportHeight};
  consts.ViewportPixelSize = {1.0f / static_cast<float>(viewportWidth), 1.0f / static_cast<float>(viewportHeight)};

  float depthLinearizeMul = (rowMajor)
                              ? (-projMatrix[3 * 4 + 2])
                              : (-projMatrix[3 + 2 *
                                             4]);     // float depthLinearizeMul = ( clipFar * clipNear ) / ( clipFar - clipNear );
  float depthLinearizeAdd = (rowMajor)
                              ? (projMatrix[2 * 4 + 2])
                              : (projMatrix[2 + 2 *
                                            4]);     // float depthLinearizeAdd = clipFar / ( clipFar - clipNear );

  // correct the handedness issue. need to make sure this below is correct, but I think it is.
  if (depthLinearizeMul * depthLinearizeAdd < 0)
    depthLinearizeAdd = -depthLinearizeAdd;
  consts.DepthUnpackConsts = {depthLinearizeMul, depthLinearizeAdd};

  float tanHalfFOVY = 1.0f / ((rowMajor)
                                ? (projMatrix[1 * 4 + 1])
                                : (projMatrix[1 + 1 *
                                              4]));    // = tanf( drawContext.Camera.GetYFOV( ) * 0.5f );
  float tanHalfFOVX = 1.0F / ((rowMajor)
                                ? (projMatrix[0 * 4 + 0])
                                : (projMatrix[0 + 0 *
                                              4]));    // = tanHalfFOVY * drawContext.Camera.GetAspect( );
  consts.CameraTanHalfFOV = {tanHalfFOVX, tanHalfFOVY};

  consts.NDCToViewMul = {consts.CameraTanHalfFOV.x * 2.0f, consts.CameraTanHalfFOV.y * -2.0f};
  consts.NDCToViewAdd = {consts.CameraTanHalfFOV.x * -1.0f, consts.CameraTanHalfFOV.y * 1.0f};

  consts.NDCToViewMul_x_PixelSize = {
    consts.NDCToViewMul.x * consts.ViewportPixelSize.x,
    consts.NDCToViewMul.y * consts.ViewportPixelSize.y
  };

  consts.EffectRadius = settings.Radius;

  consts.EffectFalloffRange = settings.FalloffRange;
  consts.DenoiseBlurBeta = (settings.DenoisePasses == 0)
                             ? (1e4f)
                             : (1.2f);    // high value disables denoise - more elegant & correct way would be do set all edges to 0

  consts.RadiusMultiplier = settings.RadiusMultiplier;
  consts.SampleDistributionPower = settings.SampleDistributionPower;
  consts.ThinOccluderCompensation = settings.ThinOccluderCompensation;
  consts.FinalValuePower = settings.FinalValuePower;
  consts.DepthMIPSamplingOffset = settings.DepthMIPSamplingOffset;
  consts.NoiseIndex = (settings.DenoisePasses > 0) ? (frameCounter % 64) : (0);
  consts.Padding0 = 0;
}
}   // close the namespace

#endif // #ifdef __cplusplus

#endif // __XE_GTAO_TYPES_H__
