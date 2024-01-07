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

#define TOTAL_PBR_MATERIAL_TEXTURE_COUNT 5
#define ALBEDO_MAP_INDEX 0
#define NORMAL_MAP_INDEX 1
#define PHYSICAL_MAP_INDEX 2
#define AO_MAP_INDEX 3
#define EMISSIVE_MAP_INDEX 4

int GetAlbedoTextureIndex(int materialIndex) {
  return materialIndex * TOTAL_PBR_MATERIAL_TEXTURE_COUNT + ALBEDO_MAP_INDEX;
}

int GetNormalTextureIndex(int materialIndex) {
  return materialIndex * TOTAL_PBR_MATERIAL_TEXTURE_COUNT + NORMAL_MAP_INDEX;
}

int GetPhysicalTextureIndex(int materialIndex) {
  return materialIndex * TOTAL_PBR_MATERIAL_TEXTURE_COUNT + PHYSICAL_MAP_INDEX;
}

int GetAOTextureIndex(int materialIndex) {
  return materialIndex * TOTAL_PBR_MATERIAL_TEXTURE_COUNT + AO_MAP_INDEX;
}

int GetEmissiveTextureIndex(int materialIndex) {
  return materialIndex * TOTAL_PBR_MATERIAL_TEXTURE_COUNT + EMISSIVE_MAP_INDEX;
}
