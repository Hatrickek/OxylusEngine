#version 450

#include "Shadows.glsl"

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

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
  int debugMode;
  float lodBias;
  ivec2 numThreads;
  ivec2 screenDimensions;
}
u_UboParams;

layout(binding = 2) buffer Lights { Light lights[]; };
// layout(binding = 3) buffer Frustums { Frustum frustums[]; };
layout(binding = 3) buffer LighIndex { int lightIndices[]; };
// layout(binding = 4) buffer LightGrid { int lightGrid[]; };

// IBL
layout(binding = 4) uniform samplerCube samplerIrradiance;
layout(binding = 5) uniform sampler2D samplerBRDFLUT;
layout(binding = 6) uniform samplerCube prefilteredMap;

// Depth
// layout(binding = 9) uniform sampler2D depthTexSampler;

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
layout(set = 1, binding = 3) uniform sampler2D metallicMap;
layout(set = 1, binding = 4) uniform sampler2D roughnessMap;

layout(location = 0) out vec4 outColor;

#include "Material.glsl"

// Normal Distribution function
float D_GGX(float dotNH, float roughness) {
  float alpha = roughness * roughness;
  float alpha2 = alpha * alpha;
  float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
  return (alpha2) / (PI * denom * denom);
}

// Geometric Shadowing function
float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness) {
  float r = (roughness + 1.0);
  float k = (r * r) / 8.0;
  float GL = dotNL / (dotNL * (1.0 - k) + k);
  float GV = dotNV / (dotNV * (1.0 - k) + k);
  return GL * GV;
}

// Fresnel function
vec3 F_Schlick(float cosTheta, vec3 F0) {
  return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}
vec3 F_SchlickR(float cosTheta, vec3 F0, float roughness) {
  return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 prefilteredReflection(vec3 R, float roughness) {
  const float MAX_REFLECTION_LOD = 9.0;  // todo: param/const
  float lod = roughness * MAX_REFLECTION_LOD;
  float lodf = floor(lod);
  float lodc = ceil(lod);
  vec3 a = textureLod(prefilteredMap, R, lodf).rgb;
  vec3 b = textureLod(prefilteredMap, R, lodc).rgb;
  return mix(a, b, lod - lodf);
}

vec3 specularContribution(vec3 L, vec3 V, vec3 N, vec3 F0, float metallic,
                          float roughness, vec3 albedo, vec3 lColor) {
  // Precalculate vectors and dot products
  vec3 H = normalize(V + L);
  float dotNH = clamp(dot(N, H), 0.0, 1.0);
  float dotNV = clamp(dot(N, V), 0.0, 1.0);
  float dotNL = clamp(dot(N, L), 0.0, 1.0);

  vec3 color = vec3(0.0);

  if (dotNL > 0.0) {
    // D = Normal distribution (Distribution of the microfacets)
    float D = D_GGX(dotNH, roughness);
    // G = Geometric shadowing term (Microfacets shadowing)
    float G = G_SchlicksmithGGX(dotNL, dotNV, roughness);
    // F = Fresnel factor (Reflectance depending on angle of incidence)
    vec3 F = F_Schlick(dotNV, F0);
    vec3 spec = D * F * G / (4.0 * dotNL * dotNV + 0.001);
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    color += (kD * albedo / PI + spec) * dotNL;
    color *= lColor;
  }

  return color;
}

// See http://www.thetenthplanet.de/archives/1180
vec3 GetNormal(vec2 uv) {
  vec3 tangentNormal = normalize(inNormal);
  if (u_Material.UseNormal) {
    tangentNormal = texture(normalMap, uv).xyz * 2.0 - 1.0;
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
  return tangentNormal;
}

void main() {
  ivec2 tileID = ivec2(gl_FragCoord.x, u_UboParams.screenDimensions.y - gl_FragCoord.y) / PIXELS_PER_TILE;
  int tileIndex = tileID.y * u_UboParams.numThreads.x + tileID.x;

  vec2 scaledUV = inUV;
  scaledUV *= u_Material.UVScale;

  float alpha = 1;
  vec3 albedo;
  if (u_Material.UseAlbedo) {
    vec4 tex4 = texture(albedoMap, scaledUV);
    alpha = tex4.a;
    albedo = pow(tex4.rgb, vec3(2.2));
    albedo *= vec3(u_Material.Color.r, u_Material.Color.g, u_Material.Color.b);
  } else {
    albedo = vec3(u_Material.Color.r, u_Material.Color.g, u_Material.Color.b);
    alpha = u_Material.Color.a;
  }

  if (alpha < 0.1) {
    discard;
  }

  vec3 normal = GetNormal(scaledUV);
  vec3 V = normalize(u_Ubo.camPos - inWorldPos);
  vec3 R = reflect(-V, normal);

  float metallic = u_Material.Metallic;
  if (u_Material.UseMetallic) {
    metallic = texture(metallicMap, scaledUV).r;
    metallic *= u_Material.Metallic;
  }

  float roughness = u_Material.Roughness;
  if (u_Material.UseRoughness) {
    roughness = texture(roughnessMap, scaledUV).r;
    roughness *= u_Material.Roughness;
  }

  vec3 F0 = vec3(0.04);
  F0 = mix(F0, albedo, metallic);

  vec3 Lo = vec3(0.0);

  vec2 brdf = texture(samplerBRDFLUT, vec2(max(dot(normal, V), 0.0), roughness)).rg;
  vec3 reflection = prefilteredReflection(R, roughness).rgb;
  vec3 irradiance = texture(samplerIrradiance, normal).rgb;

  // Diffuse based on irradiance
  vec3 diffuse = irradiance * albedo;

  vec3 F = F_SchlickR(max(dot(normal, V), 0.0), F0, roughness);

  // Specular reflectance
  vec3 specular = reflection * (F * brdf.x + brdf.y) * u_Material.Specular;

  // Ambient part
  vec3 kD = 1.0 - F;
  kD *= 1.0 - metallic;
  // vec3 ambient = (kD * diffuse + specular) * texture(aoMap, scaledUV).rrr *
  // u_Material.ao;
  vec3 ambient = (kD * diffuse + specular);
  if (u_Material.UseAO)
    ambient = (kD * diffuse + specular) * texture(aoMap, scaledUV).rrr;

  vec3 color;

  uint lightIndexBegin = tileIndex * MAX_NUM_LIGHTS_PER_TILE;
  // uint lightNum = lightGrid[tileIndex];
  vec3 viewDir = normalize(u_Ubo.camPos.xyz - inWorldPos);

  // Point lights
  for (int i = 0; i < 1; i++) {
    int lightIndex = lightIndices[i + lightIndexBegin];

    Light currentLight = lights[lightIndex];
    vec3 L = normalize(currentLight.position.xyz - inWorldPos);
    Lo += specularContribution(L, V, normal, F0, metallic, roughness, albedo,
                               currentLight.color.xyz);
    Lo *= currentLight.position.w;  // intensity
  }

  // Directional light
  // vec3 L = normalize(u_ShadowUbo.lightDir);
  // Lo += specularContribution(L, V, normal, F0, metallic, roughness, albedo,
  // vec3(1));
  //

  color = ambient + Lo;

  // Directional shadows
  uint cascadeIndex = GetCascadeIndex(u_ShadowUbo.cascadeSplits, in_ViewPos, SHADOW_MAP_CASCADE_COUNT);
  vec4 shadowCoord = GetShadowCoord(u_ShadowUbo.cascadeViewProjMat, inWorldPos, cascadeIndex);
  float shadow = FilterPCF(in_DirectShadows, shadowCoord / shadowCoord.w, cascadeIndex, ambient.r);
  color.rgb *= shadow;
  // Cascade debug
  // color *= ColorCascades(cascadeIndex);

  color += 0;//u_Material.Emmisive.rgb;


  // TODO: Make configurable via buffers
  //float far = 50.0;
  //float fogCord = (gl_FragCoord.z / gl_FragCoord.w) / far;
  //float fogDensity = 2.0;
  //float fog = fogCord * fogDensity;
  //vec4 fogColor = vec4(vec3(1), 0);
  //vec4 outFog = mix(fogColor, vec4(color, 1.0), clamp(1.0 - fog, 0.0, 1.0));

  outColor = vec4(color, 1.0);
}