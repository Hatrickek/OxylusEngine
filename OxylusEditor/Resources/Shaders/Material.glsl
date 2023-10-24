#define ALPHA_MODE_OPAQUE 0
#define ALPHA_MODE_BLEND 1
#define ALPHA_MODE_MASK 2

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
	uint AlphaMode;
	uint _pad;
};

layout(std140, set = 2, binding = 0) readonly buffer MaterialBuffer {
	Material Materials[];
};

layout(push_constant, std140) uniform PC_Material {
  layout(offset = 64) int MaterialIndex;
};
