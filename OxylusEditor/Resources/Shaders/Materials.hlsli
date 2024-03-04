#define ALPHA_MODE_OPAQUE 0
#define ALPHA_MODE_BLEND 1
#define ALPHA_MODE_MASK 2

#define SAMPLER_BILINEAR 0
#define SAMPLER_TRILINEAR 1
#define SAMPELR_ANISOTROPY 2

struct Material {
  float4 Color;
  float4 Emissive;

  float Roughness;
  float Metallic;
  float Reflectance;
  float Normal;

  float AO;
  uint32_t AlbedoMapID;
  uint32_t PhysicalMapID;
  uint32_t NormalMapID;

  uint32_t AOMapID;
  uint32_t EmissiveMapID;
  float AlphaCutoff;
  bool DoubleSided;

  float UVScale;
  uint32_t AlphaMode;
  uint32_t Sampler;
  uint32_t _pad;
};



