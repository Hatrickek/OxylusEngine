struct Material {
  vec4 Color;
  vec4 Emmisive;
  float Roughness;
  float Metallic;
  float Specular;
  float Normal;
  float AO;
  bool UseAlbedo;
  bool UsePhysicalMap;
  bool UseNormal;
  bool UseAO;
  bool UseEmissive;
  bool UseSpecular;
  float AlphaCutoff;
  bool DoubleSided;
  uint UVScale;
};

layout(std140, set = 2, binding = 0) readonly buffer MaterialBuffer {
	Material Materials[];
};

layout(push_constant, std140) uniform PC_Material {
  layout(offset = 64) int MaterialIndex;
};
