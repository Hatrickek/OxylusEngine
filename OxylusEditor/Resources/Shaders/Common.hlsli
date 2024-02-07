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

float Det2x2(float2x2 mat) {
  return mat[0][0] * mat[1][1] - mat[0][1] * mat[1][0];
}

float2x2 inverse(float2x2 mat) {
  float2x2 m;
  m[0][0] = mat[1][1];
  m[0][1] = -mat[0][1];
  m[1][0] = -mat[1][0];
  m[1][1] = mat[0][0];

  return mul((1.0f / Det2x2(mat)), m);
}

float4x4 inverse(float4x4 m) {
  float n11 = m[0][0], n12 = m[1][0], n13 = m[2][0], n14 = m[3][0];
  float n21 = m[0][1], n22 = m[1][1], n23 = m[2][1], n24 = m[3][1];
  float n31 = m[0][2], n32 = m[1][2], n33 = m[2][2], n34 = m[3][2];
  float n41 = m[0][3], n42 = m[1][3], n43 = m[2][3], n44 = m[3][3];

  float t11 = n23 * n34 * n42 - n24 * n33 * n42 + n24 * n32 * n43 - n22 * n34 * n43 - n23 * n32 * n44 + n22 * n33 * n44;
  float t12 = n14 * n33 * n42 - n13 * n34 * n42 - n14 * n32 * n43 + n12 * n34 * n43 + n13 * n32 * n44 - n12 * n33 * n44;
  float t13 = n13 * n24 * n42 - n14 * n23 * n42 + n14 * n22 * n43 - n12 * n24 * n43 - n13 * n22 * n44 + n12 * n23 * n44;
  float t14 = n14 * n23 * n32 - n13 * n24 * n32 - n14 * n22 * n33 + n12 * n24 * n33 + n13 * n22 * n34 - n12 * n23 * n34;

  float det = n11 * t11 + n21 * t12 + n31 * t13 + n41 * t14;
  float idet = 1.0f / det;

  float4x4 ret;

  ret[0][0] = t11 * idet;
  ret[0][1] = (n24 * n33 * n41 - n23 * n34 * n41 - n24 * n31 * n43 + n21 * n34 * n43 + n23 * n31 * n44 - n21 * n33 * n44) * idet;
  ret[0][2] = (n22 * n34 * n41 - n24 * n32 * n41 + n24 * n31 * n42 - n21 * n34 * n42 - n22 * n31 * n44 + n21 * n32 * n44) * idet;
  ret[0][3] = (n23 * n32 * n41 - n22 * n33 * n41 - n23 * n31 * n42 + n21 * n33 * n42 + n22 * n31 * n43 - n21 * n32 * n43) * idet;

  ret[1][0] = t12 * idet;
  ret[1][1] = (n13 * n34 * n41 - n14 * n33 * n41 + n14 * n31 * n43 - n11 * n34 * n43 - n13 * n31 * n44 + n11 * n33 * n44) * idet;
  ret[1][2] = (n14 * n32 * n41 - n12 * n34 * n41 - n14 * n31 * n42 + n11 * n34 * n42 + n12 * n31 * n44 - n11 * n32 * n44) * idet;
  ret[1][3] = (n12 * n33 * n41 - n13 * n32 * n41 + n13 * n31 * n42 - n11 * n33 * n42 - n12 * n31 * n43 + n11 * n32 * n43) * idet;

  ret[2][0] = t13 * idet;
  ret[2][1] = (n14 * n23 * n41 - n13 * n24 * n41 - n14 * n21 * n43 + n11 * n24 * n43 + n13 * n21 * n44 - n11 * n23 * n44) * idet;
  ret[2][2] = (n12 * n24 * n41 - n14 * n22 * n41 + n14 * n21 * n42 - n11 * n24 * n42 - n12 * n21 * n44 + n11 * n22 * n44) * idet;
  ret[2][3] = (n13 * n22 * n41 - n12 * n23 * n41 - n13 * n21 * n42 + n11 * n23 * n42 + n12 * n21 * n43 - n11 * n22 * n43) * idet;

  ret[3][0] = t14 * idet;
  ret[3][1] = (n13 * n24 * n31 - n14 * n23 * n31 + n14 * n21 * n33 - n11 * n24 * n33 - n13 * n21 * n34 + n11 * n23 * n34) * idet;
  ret[3][2] = (n14 * n22 * n31 - n12 * n24 * n31 - n14 * n21 * n32 + n11 * n24 * n32 + n12 * n21 * n34 - n11 * n22 * n34) * idet;
  ret[3][3] = (n12 * n23 * n31 - n13 * n22 * n31 + n13 * n21 * n32 - n11 * n23 * n32 - n12 * n21 * n33 + n11 * n22 * n33) * idet;

  return ret;
}


#endif
