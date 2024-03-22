#define ALPHA_MODE_OPAQUE 0
#define ALPHA_MODE_BLEND 1
#define ALPHA_MODE_MASK 2

#define SAMPLER_BILINEAR 0
#define SAMPLER_TRILINEAR 1
#define SAMPELR_ANISOTROPY 2

struct Material {
  float4 color;
  float4 emissive;

  float roughness;
  float metallic;
  float reflectance;
  float normal;

  float ao;
  uint32_t albedo_map_id;
  uint32_t physical_map_id;
  uint32_t normal_map_id;

  uint32_t ao_map_id;
  uint32_t emissive_map_id;
  float alpha_cutoff;
  bool double_sided;

  float uv_scale;
  uint32_t alpha_mode;
  uint32_t sampler;
  uint32_t _pad;
};



