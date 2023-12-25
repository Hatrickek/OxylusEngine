#define ALPHA_MODE_OPAQUE 0
#define ALPHA_MODE_BLEND 1
#define ALPHA_MODE_MASK 2

struct Material {
  float4 Color;
  float4 Emissive;
  float Roughness;
  float Metallic;
  float Reflectance;
  float Normal;
  float AO;
  bool UseAlbedo;
  bool UsePhysicalMap;
  bool UseNormal;
  bool UseAO;
  bool UseEmissive;
  float AlphaCutoff;
  bool DoubleSided;
  uint UVScale;
  uint AlphaMode;
  float2 _pad;
};

// set = 2 binding = 0
[[vk::binding(0, 2)]] cbuffer GTAOConstantBuffer {
  Material Materials[];
}

//@todo: we probably should offset it by the vertex 
[[vk::push_constant]] struct MaterialPushConstant {
  int MaterialIndex;
};

