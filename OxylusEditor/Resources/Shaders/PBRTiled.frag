#version 450
#pragma shader_stage(fragment)

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "Shadows.glsl"

#define PI 3.1415926535897932384626433832795
#define MAX_NUM_LIGHTS_PER_TILE 128
#define PIXELS_PER_TILE 16

struct Frustum {
  vec4 planes[4];
};

struct Light {
  vec4 position;  // w: intensity
  vec4 color;     // w: radius
};

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 in_ViewPos;

layout(binding = 0) uniform UBO {
  mat4 projection;
  mat4 view;
  vec3 camPos;
}
u_Ubo;

layout(binding = 1) uniform UBOParams {
  int numLights;
  ivec2 numThreads;
  ivec2 screenDimensions;
}
u_UboParams;

layout(binding = 2) buffer Lights { Light lights[]; };
// layout(binding = 3) buffer Frustums { Frustum frustums[]; };
//layout(binding = 3) buffer LighIndex { int lightIndices[]; };
// layout(binding = 4) buffer LightGrid { int lightGrid[]; };

// IBL
layout(binding = 4) uniform samplerCube samplerIrradiance;
layout(binding = 5) uniform sampler2D samplerBRDFLUT;
layout(binding = 6) uniform samplerCube prefilteredMap;

// Shadows
layout(binding = 7) uniform sampler2DArray in_DirectShadows;

layout(binding = 8) uniform ShadowUBO {
  mat4 cascadeViewProjMat[SHADOW_MAP_CASCADE_COUNT];
  vec4 cascadeSplits;
}
u_ShadowUbo;

// Material
layout(set = 1, binding = 0) uniform sampler2D albedoMap;
layout(set = 1, binding = 1) uniform sampler2D normalMap;
layout(set = 1, binding = 2) uniform sampler2D aoMap;
layout(set = 1, binding = 3) uniform sampler2D physicalMap;

layout(location = 0) out vec4 outColor;

#include "Material.glsl"
//#define MANUAL_SRGB 1 // we have to tonemap some inputs to make this viable
#include "SRGBToLinear.glsl"

struct PBRInfo {
  float NdotL;                  // cos angle between normal and light direction
  float NdotV;                  // cos angle between normal and view direction
  float NdotH;                  // cos angle between normal and half vector
  float LdotH;                  // cos angle between light direction and half vector
  float VdotH;                  // cos angle between view direction and half vector
  float perceptualRoughness;    // roughness value, as authored by the model creator (input to shader)
  float metalness;              // metallic value at the surface
  vec3 reflectance0;            // full reflectance color (normal incidence angle)
  vec3 reflectance90;           // reflectance color at grazing angle
  float alphaRoughness;         // roughness mapped to a more linear change in the roughness (proposed by [2])
  vec3 diffuseColor;            // color contribution from diffuse lighting
  vec3 specularColor;           // color contribution from specular lighting
};

// Basic Lambertian diffuse
// Implementation from Lambert's Photometria https://archive.org/details/lambertsphotome00lambgoog
vec3 diffuse(PBRInfo pbrInputs) {
	return pbrInputs.diffuseColor / PI;
}

vec3 SpecularReflection(PBRInfo pbrInputs) {
	return pbrInputs.reflectance0 + (pbrInputs.reflectance90 - pbrInputs.reflectance0) * pow(clamp(1.0 - pbrInputs.VdotH, 0.0, 1.0), 5.0);
}

float MicrofacetDistribution(PBRInfo pbrInputs) {
	float roughnessSq = pbrInputs.alphaRoughness * pbrInputs.alphaRoughness;
	float f = (pbrInputs.NdotH * roughnessSq - pbrInputs.NdotH) * pbrInputs.NdotH + 1.0;
	return roughnessSq / (PI * f * f);
}

float GeometricOcclusion(PBRInfo pbrInputs) {
	float NdotL = pbrInputs.NdotL;
	float NdotV = pbrInputs.NdotV;
	float r = pbrInputs.alphaRoughness;

	float attenuationL = 2.0 * NdotL / (NdotL + sqrt(r * r + (1.0 - r * r) * (NdotL * NdotL)));
	float attenuationV = 2.0 * NdotV / (NdotV + sqrt(r * r + (1.0 - r * r) * (NdotV * NdotV)));
	return attenuationL * attenuationV;
}

vec3 GetNormal(vec2 uv) {
  vec3 tangentNormal = texture(normalMap, uv).xyz * 2.0 - 1.0;

  vec3 q1 = dFdx(inWorldPos);
  vec3 q2 = dFdy(inWorldPos);
  vec2 st1 = dFdx(uv);
  vec2 st2 = dFdy(uv);

  vec3 N = normalize(inNormal);
  vec3 T = normalize(q1 * st2.t - q2 * st1.t);
  vec3 B = -normalize(cross(N, T));
  mat3 TBN = mat3(T, B, N);

  return normalize(TBN * tangentNormal);
}

vec3 GetIBLContribution(float NdotV, float perceptualRoughness, vec3 diffuseColor, vec3 specularColor, vec3 n, vec3 reflection) {
	float lod = (perceptualRoughness /* * uboParams.prefilteredCubeMipLevels*/);
	// retrieve a scale and bias to F0. See [1], Figure 3
	vec3 brdf = (texture(samplerBRDFLUT, vec2(NdotV, 1.0 - perceptualRoughness))).rgb;
	vec3 diffuseLight = SRGBtoLINEAR(texture(samplerIrradiance, n)).rgb;

	vec3 specularLight = SRGBtoLINEAR(textureLod(prefilteredMap, reflection, lod)).rgb;

	vec3 diffuse = diffuseLight * diffuseColor;
	vec3 specular = specularLight * (specularColor * brdf.x + brdf.y);

	return diffuse + specular;
}

void main() {
	ivec2 tileID = ivec2(gl_FragCoord.x, u_UboParams.screenDimensions.y - gl_FragCoord.y) / PIXELS_PER_TILE;
	int tileIndex = tileID.y * u_UboParams.numThreads.x + tileID.x;

	Material mat = Materials[MaterialIndex];
	vec2 scaledUV = inUV;
	scaledUV *= mat.UVScale;

	float perceptualRoughness;
	float metallic;
	vec3 diffuseColor;
	vec4 baseColor;

	if (mat.UseAlbedo) {
		baseColor = SRGBtoLINEAR(texture(albedoMap, scaledUV)) * mat.Color;
	} else {
		baseColor = mat.Color;
	}
	if (baseColor.a < mat.AlphaCutoff && mat.AlphaMode == ALPHA_MODE_MASK) {
		discard;
	}

	vec3 f0 = vec3(0.04);

	// Metallic and Roughness material properties are packed together
	const float c_MinRoughness = 0.04;
	perceptualRoughness = mat.Roughness;
	metallic = mat.Metallic;
	if (mat.UsePhysicalMap) {
		// Roughness is stored in the 'g' channel, metallic is stored in the 'b' channel.
		// This layout intentionally reserves the 'r' channel for (optional) occlusion map data
		vec4 mrSample = texture(physicalMap, scaledUV);
		perceptualRoughness = mrSample.g * perceptualRoughness;
		metallic = mrSample.b * metallic;
	} else {
		perceptualRoughness = clamp(perceptualRoughness, c_MinRoughness, 1.0);
		metallic = clamp(metallic, 0.0, 1.0);
	}

	diffuseColor = baseColor.rgb * (vec3(1.0) - f0);
	diffuseColor *= 1.0 - metallic;
		
	float alphaRoughness = perceptualRoughness * perceptualRoughness;

	vec3 specularColor = mix(f0, baseColor.rgb, metallic);

	// Compute reflectance.
	float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);

	// For typical incident reflectance range (between 4% to 100%) set the grazing reflectance to 100% for typical fresnel effect.
	// For very low reflectance range on highly diffuse objects (below 4%), incrementally reduce grazing reflecance to 0%.
	float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);
	vec3 specularEnvironmentR0 = specularColor.rgb;
	vec3 specularEnvironmentR90 = vec3(1.0, 1.0, 1.0) * reflectance90;

	uint lightIndexBegin = tileIndex * MAX_NUM_LIGHTS_PER_TILE;
	// uint lightNum = lightGrid[tileIndex];
	vec3 color = vec3(0);

	// Point lights
	vec3 n = mat.UseNormal ? GetNormal(scaledUV) : normalize(inNormal);
	vec3 v = normalize(u_Ubo.camPos - inWorldPos);		// Vector from surface point to camera
	float NdotV = clamp(abs(dot(n, v)), 0.001, 1.0);
	vec3 reflection = -normalize(reflect(v, n));
	for (int i = 0; i < u_UboParams.numLights; i++) {
		int lightIndex = i;//lightIndices[i + lightIndexBegin];

		Light current_light = lights[lightIndex];
		vec3 l = normalize(current_light.position.xyz - inWorldPos);
		vec3 h = normalize(l+v);							// Half vector between both l and v

		float NdotL = clamp(dot(n, l), 0.001, 1.0);
		float NdotH = clamp(dot(n, h), 0.0, 1.0);
		float LdotH = clamp(dot(l, h), 0.0, 1.0);
		float VdotH = clamp(dot(v, h), 0.0, 1.0);

		PBRInfo pbrInputs = PBRInfo(
			NdotL,
			NdotV,
			NdotH,
			LdotH,
			VdotH,
			perceptualRoughness,
			metallic,
			specularEnvironmentR0,
			specularEnvironmentR90,
			alphaRoughness,
			diffuseColor,
			specularColor
		);

		// Calculate the shading terms for the microfacet specular shading model
		vec3 F = SpecularReflection(pbrInputs);
		float G = GeometricOcclusion(pbrInputs);
		float D = MicrofacetDistribution(pbrInputs);

		const vec3 u_LightColor = vec3(1.0);

		// Calculation of analytical lighting contribution
		vec3 diffuseContrib = (1.0 - F) * diffuse(pbrInputs);
		vec3 specContrib = F * G * D / (4.0 * NdotL * NdotV);
		specContrib *= current_light.position.w;
		// Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
		color = NdotL * u_LightColor * (diffuseContrib + specContrib);
	}

	// Calculate lighting contribution from image based lighting source (IBL)
	color += GetIBLContribution(NdotV, perceptualRoughness, diffuseColor, specularColor, n, reflection);

	const float u_OcclusionStrength = 1.0f;
	// Apply optional PBR terms for additional (optional) shading
	if (mat.UseAO) {
		float ao = texture(aoMap, scaledUV).r;
		color = mix(color, color * ao, u_OcclusionStrength);
	}

	const float u_EmissiveFactor = 1.0f;
	if (mat.UseEmissive) {
		// TODO: vec3 emissive = SRGBtoLINEAR(texture(emissiveMap, scaledUV).rgb * u_EmissiveFactor;
		// color += emissive;
	}

	// Directional shadows
	uint cascadeIndex = GetCascadeIndex(u_ShadowUbo.cascadeSplits, in_ViewPos, SHADOW_MAP_CASCADE_COUNT);
	vec4 shadowCoord = GetShadowCoord(u_ShadowUbo.cascadeViewProjMat, inWorldPos, cascadeIndex);
	float shadow = FilterPCF(in_DirectShadows, shadowCoord / shadowCoord.w, cascadeIndex, color.r);
	color.rgb *= shadow;

	// Cascade debug
	// color *= ColorCascades(cascadeIndex);
	
	outColor = vec4(color, baseColor.a);
}