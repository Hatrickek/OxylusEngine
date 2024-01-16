#define PI 3.1415926535897932384626433832795
#define TwoPI = 2 * PI;
#define Epsilon = 0.00001;

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
  float4x4 ViewMatrix;
  float4x4 InvProjectionViewMatrix;
};

#define DIRECTIONAL_LIGHT 0
#define POINT_LIGHT 1
#define SPOT_LIGHT 2

#define SHADOW_MAP_CASCADE_COUNT 4

struct Light {
  float4 PositionIntensity; // w: intensity
  float4 ColorRadius;       // w: radius
  float4 RotationType;      // w: type
};

struct SceneData {
  int NumLights;
  int _pad1;
  int2 ScreenSize;
  
  float3 SunDirection;
  int _pad2;
  float4 SunColor; // pre-multipled with intensity

  float4x4 CascadeViewProjections[4];
  float4 CascadeSplits;
  float4 ScissorNormalized;

  struct Indices {
    int CubeMapIndex;
    int PrefilteredCubeMapIndex;
    int IrradianceMapIndex;
    int BRDFLUTIndex;

    int SkyTransmittanceLutIndex;
    int SkyMultiscatterLutIndex;
    int ShadowArrayIndex;
    int GTAOIndex;
    int SSRIndex;
    float3 _pad;
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
};
