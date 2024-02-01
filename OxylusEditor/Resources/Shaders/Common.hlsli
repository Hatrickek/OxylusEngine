#ifndef COMMON_HLSLI
#define COMMON_HLSLI

#define UINT32_MAX 4294967295
#define INVALID_ID UINT32_MAX
#define PI 3.1415926535897932384626433832795
#define TwoPI 2 * PI
#define EPSILON 0.00001

#define MEDIUMP_FLT_MAX    65504.0
#define saturateMediump(x) min(x, MEDIUMP_FLT_MAX)
#define sqr(a) ((a)*(a))
#define pow5(x) pow(x, 5)

struct Vertex {
  float3 Position : POSITION;
  int _pad : PAD0;
  float3 Normal : NORMAL;
  int _pad2 : PAD1;
  float2 UV : TEXCOORD0;
  float2 _pad3 : PAD2;
  float4 Tangent : TEXCOORD1;
  float4 Color : TEXCOORD2;
  float4 Joint0 : TEXCOORD3;
  float4 Weight0 : TEXCOORD4;
};

struct CameraData {
  float4 Position;
  float4x4 ProjectionMatrix;
  float4x4 InvProjectionMatrix;
  float4x4 ViewMatrix;
  float4x4 InvViewMatrix;
  float4x4 InvProjectionViewMatrix;
  float NearClip;
  float FarClip;
  float Fov;
  float _pad;
};

#define DIRECTIONAL_LIGHT 0
#define POINT_LIGHT 1
#define SPOT_LIGHT 2

#define SHADOW_MAP_CASCADE_COUNT 4

struct Light {
  float3 Position;
  float Intensity;
  float3 Color;
  float Radius;
  float3 Rotation;
  uint32_t Type;
};

struct SceneData {
  int NumLights;
  float GridMaxDistance;
  int2 ScreenSize;

  float3 SunDirection;
  int _pad2;
  float4 SunColor; // pre-multipled with intensity

  float4x4 CubemapViewProjections[6];

  float4x4 CascadeViewProjections[4];
  float4 CascadeSplits;
  float4 ScissorNormalized;

  struct Indices {
    int PBRImageIndex;
    int NormalImageIndex;
    int DepthImageIndex;
    int _pad;

    int SkyEnvMapIndex;
    int SkyTransmittanceLutIndex;
    int SkyMultiscatterLutIndex;
    int ShadowArrayIndex;

    int GTAOBufferImageIndex;
    int SSRBufferImageIndex;
    int LightsBufferIndex;
    int MaterialsBufferIndex;
  } Indices;

  struct PostProcessingData {
    int Tonemapper;
    float Exposure;
    float Gamma;
    int _pad;

    int EnableBloom;
    int EnableSSR;
    int EnableGTAO;
    int _pad2;

    float4 VignetteColor;       // rgb: color, a: intensity
    float4 VignetteOffset;      // xy: offset, z: useMask, w: enable effect
    float2 FilmGrain;           // x: enable, y: amount
    float2 ChromaticAberration; // x: enable, y: amount
    float2 Sharpen;             // x: enable, y: amount
  } PostProcessingData;

  float2 _pad;
};

struct SSRData {
  int Samples;
  int BinarySearchSamples;
  float MaxDist;
  int _pad;
};

bool IsNaN(float3 vec) {
  return (asuint(vec.x) & 0x7fffffff) > 0x7f800000
         || (asuint(vec.y) & 0x7fffffff) > 0x7f800000
         || (asuint(vec.z) & 0x7fffffff) > 0x7f800000;
}

#endif
