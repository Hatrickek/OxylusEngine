#define PI 3.1415926535897932384626433832795
#define TwoPI = 2 * PI;
#define Epsilon = 0.00001;

#define PBR_TEXTURES_COUNT 5

#define ALBEDO_MAP_INDEX 0
#define NORMAL_MAP_INDEX 1
#define AO_MAP_INDEX 2
#define PHYSICAL_MAP_INDEX 3
#define EMISSIVE_MAP_INDEX 4

struct Vertex {
  float3 Position;
  float3 Normal;
  float2 UV;
  float4 Tangent;
  float4 Color;
  float4 Joint0;
  float4 Weight0;
};

struct CameraData {
  float4x4 ProjectionMatrix;
  float4x4 ViewMatrix;
  float4 Position;
};

#define DIRECTIONAL_LIGHT 0
#define POINT_LIGHT 1
#define SPOT_LIGHT 2

struct Light {
  float4 PositionIntensity; // w: intensity
  float4 ColorRadius;       // w: radius
  float4 RotationType;      // w: type
};

struct SceneData {
  int NumLights;
  Light Lights[];
};
