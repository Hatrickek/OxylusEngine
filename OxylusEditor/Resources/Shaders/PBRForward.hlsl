#include "PBRCommon.hlsli"
#include "Atmosphere/SkyCommonShading.hlsli"

[[vk::push_constant]] struct PushConstant {
  float4x4 ModelMatrix;
  uint64_t VertexBufferPtr;
  uint32_t MaterialIndex;
  float _pad;
} PushConst;

struct VSLayout {
  float4 Position : SV_POSITION;
  float3 WorldPos : WORLD_POSITION;
  float3 Normal : NORMAL;
  float2 UV : TEXCOORD;
  float3 ViewPos : VIEW_POS;
  float4 Tangent : TANGENT;
};

VSLayout VSmain(uint vertexIndex : SV_VertexID) {
  uint64_t addressOffset = PushConst.VertexBufferPtr + vertexIndex * sizeof(Vertex);
  const float4 vertexPosition = vk::RawBufferLoad<float4>(addressOffset);
  const float4 vertexNormal = vk::RawBufferLoad<float4>(addressOffset + sizeof(float4));
  const float4 vertexUV = vk::RawBufferLoad<float4>(addressOffset + sizeof(float4) * 2);
  const float4 tangent = vk::RawBufferLoad<float4>(addressOffset + sizeof(float4) * 3);

  const float4 locPos = mul(PushConst.ModelMatrix, float4(vertexPosition.xyz, 1.0f));

  VSLayout output;
  output.Normal = normalize(mul(transpose(PushConst.ModelMatrix), vertexNormal)).xyz;
  output.WorldPos = locPos.xyz / locPos.w;
  output.ViewPos = mul(GetCamera().ViewMatrix, float4(locPos.xyz, 1.0f)).xyz;
  output.UV = vertexUV.xy;
  output.Position = mul(mul(GetCamera().ProjectionMatrix, GetCamera().ViewMatrix), float4(output.WorldPos, 1.0f));
  output.Tangent = mul(PushConst.ModelMatrix, tangent);

  return output;
}

float D_GGX(float roughness, float NoH, const float3 h) {
  // Walter et al. 2007, "Microfacet Models for Refraction through Rough Surfaces"
  float oneMinusNoHSquared = 1.0 - NoH * NoH;

  float a = NoH * roughness;
  float k = roughness / (oneMinusNoHSquared + a * a);
  float d = k * k * (1.0 / PI);
  return saturateMediump(d);
}

float V_SmithGGXCorrelated(float roughness, float NoV, float NoL) {
  // Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"
  float a2 = roughness * roughness;
  // TODO: lambdaV can be pre-computed for all the lights, it should be moved out of this function
  float lambdaV = NoL * sqrt((NoV - a2 * NoV) * NoV + a2);
  float lambdaL = NoV * sqrt((NoL - a2 * NoL) * NoL + a2);
  float v = 0.5 / (lambdaV + lambdaL);
  // a2=0 => v = 1 / 4*NoL*NoV   => min=1/4, max=+inf
  // a2=1 => v = 1 / 2*(NoL+NoV) => min=1/4, max=+inf
  // clamp to the maximum value representable in mediump
  return saturateMediump(v);
}

// https://www.unrealengine.com/en-US/blog/physically-based-shading-on-mobile
half3 EnvBRDFApprox(half3 SpecularColor, half Roughness, half NoV) {
  const half4 c0 = {-1, -0.0275, -0.572, 0.022};
  const half4 c1 = {1, 0.0425, 1.04, -0.04};
  half4 r = Roughness * c0 + c1;
  half a004 = min(r.x * r.x, exp2(-9.28 * NoV)) * r.x + r.y;
  half2 AB = half2(-1.04, 1.04) * a004 + r.zw;
  return SpecularColor * AB.x + AB.y;
}

struct AnisotropicSurface {
  float2 direction;
  float strength;
  float3 T;

  // computed values:
  float at;
  float ab;
  float3 B;
  float TdotV;
  float BdotV;
};

struct Surface {
  float4 BaseColor;
  float3 Albedo;
  float Metallic;
  float Roughness;
  float Occlusion;		  // occlusion [0 -> 1]
  float4 Refraction;
  float3 EmissiveColor;
  float3 P;             // world space position
  float3 VP;            // view space position
  float3 N;             // world space normal
  float3 V;             // world space view vector
  float4 T;				      // tangent
  float3 B;				      // bitangent
  float H;
  float3 F0;            // fresnel value
  float2 PixelPosition;
  float Opacity;			  // opacity for blending operation [0 -> 1]
  float3 ViewPos;
  float3 BumpColor;

#ifdef SHEEN
	SheenSurface sheen;
#endif // SHEEN

#ifdef CLEARCOAT
	ClearcoatSurface clearcoat;
#endif // CLEARCOAT

#ifdef ANISOTROPIC
	AnisotropicSurface aniso;
#endif // ANISOTROPIC

  float NdotV; // cos(angle between normal and view vector)
  float3 R; // reflection vector
  float3 F; // fresnel term computed from NdotV

  void Init() {
    P = 0;
    V = 0;
    N = 0;
    BaseColor = 1;
    Albedo = 1;
    F0 = 0.0;
    Roughness = 1;
    Occlusion = 1;
    Opacity = 1;
    EmissiveColor = 0;
    Refraction = 0.0;
    T = 0;
    B = 0;
    BumpColor = 0;
  }

  void Create(Material material, float4 baseColor, float2 scaledUV) {
    BaseColor = baseColor;
    Opacity = baseColor.a;

    float4 specularMap = 1;
    F0 = specularMap.rgb * specularMap.a;

    float4 surfaceMap = 1;
    if (material.PhysicalMapID != INVALID_ID) {
      surfaceMap = GetMaterialPhysicalTexture(material).Sample(LINEAR_REPEATED_SAMPLER, scaledUV);
    }

    surfaceMap.g *= material.Roughness;
    surfaceMap.b *= material.Metallic;
    surfaceMap.a *= material.Reflectance;

    // metallic roughness workflow
    Roughness = surfaceMap.g;
    const float metalness = surfaceMap.b * material.Metallic;
    const float reflectance = surfaceMap.a;
    Albedo = baseColor.rgb * (1.0f - max(reflectance, metalness));
    F0 *= lerp(reflectance.xxx, baseColor.rgb, metalness);

    if (material.AOMapID != INVALID_ID)
      Occlusion = GetMaterialAOTexture(material).Sample(LINEAR_REPEATED_SAMPLER, scaledUV).r;
    else
      Occlusion = 1.0;
  }

  void Update() {
    Roughness = saturate(Roughness);
    NdotV = saturate(dot(N, V) + 1e-5);
    F = EnvBRDFApprox(F0, Roughness, NdotV);
    R = -reflect(V, N);
    B = normalize(cross(T.xyz, N) * T.w); // Compute bitangent again after normal mapping
#ifdef SHEEN
		sheen.roughness = saturate(sheen.roughness);
    // Sheen energy compensation: https://dassaultsystemes-technology.github.io/EnterprisePBRShadingModel/spec-2021x.md.html#figure_energy-compensation-sheen-e
		sheen.DFG = GetSheenLUT().SampleLevel(LINEAR_CLAMPED_SAMPLER, float2(NdotV, sheen.roughness), 0).r;
		sheen.albedoScaling = 1.0 - max3(sheen.color) * sheen.DFG;
#endif
#ifdef CLEARCOAT
		clearcoat.roughness = saturate(clearcoat.roughness);
    clearcoat.F = EnvBRDFApprox(f0, clearcoat.roughness, clearcoatNdotV);
    clearcoat.F *= clearcoat.factor;
		clearcoat.R = -reflect(V, clearcoat.N);
#endif
#ifdef ANISOTROPIC
		aniso.B = normalize(cross(N, aniso.T));
		aniso.TdotV = dot(aniso.T.xyz, V);
		aniso.BdotV = dot(aniso.B, V);
		float roughnessBRDF = sqr(clamp(roughness, 0.045, 1));
		aniso.at = max(0, roughnessBRDF * (1 + aniso.strength));
		aniso.ab = max(0, roughnessBRDF * (1 - aniso.strength));
#endif // ANISOTROPIC
  }
};

struct SurfaceToLight {
  float3 L; // surface to light vector (normalized)
  float3 H; // half-vector between view vector and light vector
  float NdotL; // cos angle between normal and light direction
  float NdotH; // cos angle between normal and half vector
  float LdotH; // cos angle between light direction and half vector
  float VdotH; // cos angle between view direction and half vector
  float3 F; // fresnel term computed from VdotH

#ifdef ANISOTROPIC
	float TdotL;
	float BdotL;
	float TdotH;
	float BdotH;
#endif // ANISOTROPIC

  void Create(Surface surface, float3 Lnormalized) {
    L = Lnormalized;
    H = normalize(L + surface.V);

    NdotL = dot(L, surface.N);

#ifdef BRDF_NDOTL_BIAS
		NdotL += BRDF_NDOTL_BIAS;
#endif // BRDF_NDOTL_BIAS

    NdotH = saturate(dot(surface.N, H));
    LdotH = saturate(dot(L, H));
    VdotH = saturate(dot(surface.V, H));

    F = F_Schlick(surface.F0, VdotH);

#ifdef ANISOTROPIC
		TdotL = dot(surface.aniso.T.xyz, L);
		BdotL = dot(surface.aniso.B, L);
		TdotH = dot(surface.aniso.T.xyz, H);
		BdotH = dot(surface.aniso.B, H);
#endif

    NdotL = saturate(NdotL);
  }
};

float3 BRDF_GetSpecular(Surface surface, SurfaceToLight surface_to_light) {
#ifdef ANISOTROPIC
	float D = D_GGX_Anisotropic(surface.aniso.at, surface.aniso.ab, surface_to_light.TdotH, surface_to_light.BdotH, surface_to_light.NdotH);
	float Vis = V_SmithGGXCorrelated_Anisotropic(surface.aniso.at, surface.aniso.ab, surface.aniso.TdotV, surface.aniso.BdotV,
	surface_to_light.TdotL, surface_to_light.BdotL, surface.NdotV, surface_to_light.NdotL);
#else
  float roughnessBRDF = sqr(clamp(surface.Roughness, 0.045f, 1.0f));
  float D = D_GGX(roughnessBRDF, surface_to_light.NdotH, surface_to_light.H);
  float Vis = V_SmithGGXCorrelated(roughnessBRDF, surface.NdotV, surface_to_light.NdotL);
#endif

  float3 specular = D * Vis * surface_to_light.F;

#ifdef SHEEN
	specular *= surface.sheen.albedoScaling;
	float sheen_roughnessBRDF = sqr(clamp(surface.sheen.roughness, 0.045, 1));
	D = D_Charlie(sheen_roughnessBRDF, surface_to_light.NdotH);
	Vis = V_Neubelt(surface.NdotV, surface_to_light.NdotL);
	specular += D * Vis * surface.sheen.color;
#endif

#ifdef CLEARCOAT
	specular *= 1 - surface.clearcoat.F;
	float NdotH = saturate(dot(surface.clearcoat.N, surface_to_light.H));
	float clearcoat_roughnessBRDF = sqr(clamp(surface.clearcoat.roughness, 0.045, 1));
	D = D_GGX(clearcoat_roughnessBRDF, NdotH, surface_to_light.H);
	Vis = V_Kelemen(surface_to_light.LdotH);
	specular += D * Vis * surface.clearcoat.F;
#endif

  return specular * surface_to_light.NdotL;
}

float3 BRDF_GetDiffuse(Surface surface, SurfaceToLight surface_to_light) {
  float3 diffuse = (1 - lerp(surface_to_light.F, 0, saturate(0.0f)));

#ifdef SHEEN
	diffuse *= surface.sheen.albedoScaling;
#endif // SHEEN

#ifdef CLEARCOAT
	diffuse *= 1 - surface.clearcoat.F;
#endif // CLEARCOAT

  return diffuse * surface_to_light.NdotL;
}

struct LightingPart {
  float3 diffuse;
  float3 specular;
};

struct Lighting {
  LightingPart direct;
  LightingPart indirect;

  void Create(
    in float3 diffuseDirect,
    in float3 specularDirect,
    in float3 diffuseIndirect,
    in float3 specularIndirect
  ) {
    direct.diffuse = diffuseDirect;
    direct.specular = specularDirect;
    indirect.diffuse = diffuseIndirect;
    indirect.specular = specularIndirect;
  }
};

inline void ApplyLighting(Surface surface, Lighting lighting, inout float4 color) {
  const float3 diffuse = lighting.direct.diffuse / PI + lighting.indirect.diffuse * (1 - surface.F) * surface.Occlusion;
  const float3 specular = lighting.direct.specular + lighting.indirect.specular * surface.Occlusion; // reminder: cannot apply surface.F for whole indirect specular, because multiple layers have separate fresnels (sheen, clearcoat)
  color.rgb = lerp(surface.Albedo * diffuse, surface.Refraction.rgb, surface.Refraction.a);
  color.rgb += specular;
  color.rgb += surface.EmissiveColor;
}

static const float4x4 BiasMatrix = float4x4(0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 0.5f, 0.0f, 1.0f);

float4 GetShadowPosition(float3 wsPos, int cascadeIndex) {
  return mul(mul(transpose(BiasMatrix), GetScene().CascadeViewProjections[cascadeIndex]), float4(wsPos, 1.0));
}

void LightDirectional(Light light, Surface surface, inout Lighting lighting) {
  float3 L = normalize(light.Rotation);

  SurfaceToLight surfaceToLight = (SurfaceToLight)0;
  surfaceToLight.Create(surface, L);

  int cascadeIndex = GetCascadeIndex(GetScene().CascadeSplits, surface.ViewPos, SHADOW_MAP_CASCADE_COUNT);
  float4 shadowPosition = GetShadowPosition(surface.P, cascadeIndex);
  float3 shadow = 1.0 - Shadow(surface.PixelPosition,
                               true,
                               GetSunShadowArrayTexture(),
                               cascadeIndex,
                               shadowPosition,
                               GetScene().ScissorNormalized);

#if 0
    // shadowFarAttenuation
    const float shadowFar = 100.0f;
    float z = dot(transpose(GetCamera().ViewMatrix)[2].xyz, surface.V);
    float2 p = shadowFar > 0.0f ? 0.5f * float2(10.0f, 10.0f / (shadowFar * shadowFar)) : float2(1.0f, 0.0f);
    shadow = 1.0f - ((1.0f - shadow) * saturate(p.x - z * z * p.y));
#endif

  if (any(shadow)) {
    float3 light_color = light.Color * shadow;

    AtmosphereParameters atmosphere = (AtmosphereParameters)0;
    InitAtmosphereParameters(atmosphere);
    light_color *= GetAtmosphericLightTransmittance(atmosphere, surface.P.xyz, L, GetSkyTransmittanceLUTTexture());

    lighting.direct.diffuse = mad(light_color, BRDF_GetDiffuse(surface, surfaceToLight), lighting.direct.diffuse);
    lighting.direct.specular = mad(light_color, BRDF_GetSpecular(surface, surfaceToLight), lighting.direct.specular);
  }
}

float AttenuationPointLight(in float dist, in float dist2, in float range, in float range2) {
  // GLTF recommendation: https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_lights_punctual#range-property
  //return saturate(1 - pow(dist / range, 4)) / dist2;

  // Removed pow(x, 4), and avoid zero divisions:
  float dist_per_range = dist2 / max(0.0001, range2); // pow2
  dist_per_range *= dist_per_range; // pow4
  return saturate(1 - dist_per_range) / max(0.0001, dist2);
}

void LightPoint(Light light, Surface surface, inout Lighting lighting) {
  float3 L = light.Position - surface.P;
  float dist = length(L);

  const float dist2 = dot(L, L);
  const float range = light.Radius;
  const float range2 = range * range;

  if (dist2 < range2) {
    SurfaceToLight surface_to_light = (SurfaceToLight)0;
    surface_to_light.Create(surface, L);

    if (any(surface_to_light.NdotL)) {
      float3 light_color = light.Color;
      light_color *= AttenuationPointLight(dist, dist2, range, range2);

      lighting.direct.diffuse = mad(light_color, BRDF_GetDiffuse(surface, surface_to_light), lighting.direct.diffuse);
      lighting.direct.specular = mad(light_color, BRDF_GetSpecular(surface, surface_to_light), lighting.direct.specular);
    }
  }
}

inline float AttenuationSpotLight(in float dist, in float dist2, in float range, in float range2, in float spot_factor, in float angle_scale, in float angle_offset) {
  float attenuation = AttenuationPointLight(dist, dist2, range, range2);
  float angularAttenuation = saturate(mad(spot_factor, angle_scale, angle_offset));
  angularAttenuation *= angularAttenuation;
  attenuation *= angularAttenuation;
  return attenuation;
}

inline void LightSpot(Light light, Surface surface, inout Lighting lighting) {
  float3 L = light.Position - surface.P;
  const float dist2 = dot(L, L);
  const float range = light.Radius;
  const float range2 = range * range;

  if (dist2 < range2) {
    const float dist = sqrt(dist2);
    L /= dist;

    SurfaceToLight surface_to_light;
    surface_to_light.Create(surface, L);

#if 0 // TODO: need more parameters (angles)
    if (any(surface_to_light.NdotL)) {
      const float spot_factor = dot(L, light.RotationType.xyz);
      const float spot_cutoff = light.GetConeAngleCos();

      if (spot_factor > spot_cutoff) {
        if (any(shadow)) {
          float3 light_color = light.ColorRadius.rgb;
          light_color *= AttenuationSpotLight(dist, dist2, range, range2, spot_factor, light.GetAngleScale(), light.GetAngleOffset());

          lighting.direct.diffuse = mad(light_color, BRDF_GetDiffuse(surface, surface_to_light), lighting.direct.diffuse);
          lighting.direct.specular = mad(light_color, BRDF_GetSpecular(surface, surface_to_light), lighting.direct.specular);
        }
      }
    }
#endif
  }
}

inline void ForwardLighting(inout Surface surface, inout Lighting lighting) {
  for (int i = 0; i < GetScene().NumLights; i++) {
    Light light = GetLight(i);

    switch (light.Type) {
      case DIRECTIONAL_LIGHT: {
        LightDirectional(light, surface, lighting);
      }
      break;
      case POINT_LIGHT: {
        LightPoint(light, surface, lighting);
      }
      break;
      case SPOT_LIGHT: {
        LightSpot(light, surface, lighting);
      }
      break;
    }
  }
}

float3x3 GetNormalTangent(VSLayout vs, float2 uv) {
  float3 q1 = ddx(vs.WorldPos);
  float3 q2 = ddy(vs.WorldPos);
  float2 st1 = ddx(uv);
  float2 st2 = ddy(uv);

  float3 T = normalize(q1 * st2.y - q2 * st1.y);
  float3 B = -normalize(cross(vs.Normal, T));
  float3x3 TBN = float3x3(T, B, vs.Normal);

  return TBN;
}

float GeometricOcclusion(Surface pixelData, float NoL) {
  float NdotL = NoL;
  float NdotV = pixelData.NdotV;
  float r = pixelData.Roughness * pixelData.Roughness;

  float attenuationL = 2.0 * NdotL / (NdotL + sqrt(r * r + (1.0 - r * r) * (NdotL * NdotL)));
  float attenuationV = 2.0 * NdotV / (NdotV + sqrt(r * r + (1.0 - r * r) * (NdotV * NdotV)));
  return attenuationL * attenuationV;
}

float3 GetAmbient(float3 worldNormal) {
  // Set realistic_sky_stationary to true so we capture ambient at float3(0.0, 0.0, 0.0), similar to the standard sky to avoid flickering and weird behavior
  float3 ambient = lerp(
    GetDynamicSkyColor(float2(0.0f, 0.0f), float3(0, -1, 0), false, false, true),
    GetDynamicSkyColor(float2(0.0f, 0.0f), float3(0, 1, 0), false, false, true),
    saturate(worldNormal.y * 0.5 + 0.5)
  );

  ambient += float3(0.19608, 0.19608, 0.19608); // GetScene().AmbientColor;

  return ambient;
}

float4 PSmain(VSLayout input, float4 pixelPosition : SV_Position) : SV_Target {
  Material material = GetMaterial(PushConst.MaterialIndex);
  float2 scaledUV = input.UV;
  scaledUV *= material.UVScale;

  float4 baseColor;

  if (material.AlbedoMapID != INVALID_ID) {
    baseColor = GetMaterialAlbedoTexture(material).Sample(LINEAR_REPEATED_SAMPLER, scaledUV) * material.Color;
  }
  else {
    baseColor = material.Color;
  }
  if (baseColor.a < material.AlphaCutoff && material.AlphaMode == ALPHA_MODE_MASK) {
    discard;
  }

  float3 emissive = float3(0.0, 0.0, 0.0);
  if (material.EmissiveMapID != INVALID_ID) {
    float3 value = GetMaterialEmissiveTexture(material).Sample(LINEAR_REPEATED_SAMPLER, scaledUV).rgb;
    emissive += value;
  }
  else {
    emissive += material.Emissive.rgb * material.Emissive.a;
  }

  Surface surface = (Surface)0;
  surface.Init();
  surface.P = input.WorldPos.xyz;
  surface.N = normalize(input.Normal);
  surface.V = GetCamera().Position.xyz - surface.P;
  surface.V /= length(surface.V);
  surface.PixelPosition = pixelPosition.xy;
  surface.EmissiveColor = emissive;
  surface.ViewPos = input.ViewPos;
  surface.Create(material, baseColor, scaledUV);

  if (material.NormalMapID != INVALID_ID) {
    surface.BumpColor = float3(GetMaterialNormalTexture(material).Sample(LINEAR_REPEATED_SAMPLER, scaledUV).rg, 1.0);
    surface.BumpColor = surface.BumpColor * 2.f - 1.f;
  }

  surface.T = input.Tangent;
  surface.T.xyz = normalize(surface.T.xyz);
  float3x3 TBN = GetNormalTangent(input, scaledUV);

  if (any(surface.BumpColor)) {
    surface.N = normalize(lerp(surface.N, mul(surface.BumpColor, TBN), length(surface.BumpColor)));
  }

  float2 screenCoords = pixelPosition.xy / float2(GetScene().ScreenSize.x, GetScene().ScreenSize.y);

#ifndef TRANSPARENT
  // Apply GTAO
  if (GetScene().PostProcessingData.EnableGTAO == 1) {
    uint value = GetGTAOTexture().Sample(NEAREST_REPEATED_SAMPLER, screenCoords).r;
    float aoVisibility = value / 255.0;
    surface.Occlusion *= aoVisibility;
  }
#endif

#ifdef ANISOTROPIC
	surface.aniso.strength = material.anisotropy_strength;
	surface.aniso.direction = float2(material.anisotropy_rotation_cos, material.anisotropy_rotation_sin);

#ifdef OBJECTSHADER_USE_UVSETS
	if (material.AnistropyMapID != INVALID_ID)
	{
		float2 anisotropyTexture = GetMaterialAnistropyTexture(material).Sample(LINEAR_REPEATED_SAMPLER, scaledUV).rg * 2 - 1;
		surface.aniso.strength *= length(anisotropyTexture);
		surface.aniso.direction = mul(float2x2(surface.aniso.direction.x, surface.aniso.direction.y, -surface.aniso.direction.y, surface.aniso.direction.x), normalize(anisotropyTexture));
	}
#endif // OBJECTSHADER_USE_UVSETS

	surface.aniso.T = normalize(mul(TBN, float3(surface.aniso.direction, 0)));

#endif // ANISOTROPIC

#ifdef SHEEN
	surface.sheen.color = material.SheenColor;
	surface.sheen.roughness = material.SheenRoughness;

	if (material.SheenColorMapID != INVALID_ID) {
		surface.sheen.color = GetMaterialSheenColorTexture(material).Sample(LINEAR_REPEATED_SAMPLER, scaledUV).rgb;
	}
	if (material.SheenRoughnessMapID != INVALID_ID) {
		surface.sheen.roughness = GetMaterialSheenRoughnessTexture(material).Sample(LINEAR_REPEATED_SAMPLER, scaledUV).a;
	}
#endif // SHEEN

#ifdef CLEARCOAT
	surface.clearcoat.factor = material.Clearcoat;
	surface.clearcoat.roughness = material.ClearcoatRoughness;
	surface.clearcoat.N = input.nor;

	if (material.ClearCoatMapID != INVALID_ID) {
		surface.clearcoat.factor = GetMaterialClearCoatTexture(material).Sample(LINEAR_REPEATED_SAMPLER, scaledUV).r;
	}
	if (material.ClearCoatRoughnessMapID != INVALID_ID) {
		surface.clearcoat.roughness = GetMaterialClearCoatRoughnessTexture(material).Sample(LINEAR_REPEATED_SAMPLER, scaledUV).g;
	}
#ifdef OBJECTSHADER_USE_TANGENT
	if (material.ClearCoatNormalMapID != INVALID_ID) {
		float3 clearcoatNormalMap = float3(GetMaterialClearCoatNormalTexture(material).Sample(LINEAR_REPEATED_SAMPLER, scaledUV).rg, 1);
		clearcoatNormalMap = clearcoatNormalMap * 2 - 1;
		surface.clearcoat.N = mul(clearcoatNormalMap, TBN);
	}
#endif // OBJECTSHADER_USE_TANGENT

	surface.clearcoat.N = normalize(surface.clearcoat.N);

#endif // CLEARCOAT

  surface.Update();

  float3 ambient = GetAmbient(surface.N);

  TextureCube cubemap = GetSkyEnvMapTexture();
  uint2 dim;
  uint mips;
  cubemap.GetDimensions(0, dim.x, dim.y, mips);

  float MIP = surface.Roughness * mips;
  float3 envColor = cubemap.SampleLevel(LINEAR_CLAMPED_SAMPLER, surface.R, 0).rgb * surface.F;
   
  Lighting lighting;
  lighting.Create(0, 0, ambient, max(0, envColor));

  float4 color = surface.BaseColor;

  ForwardLighting(surface, lighting);

#if 0
  if (GetScene().PostProcessingData.EnableSSR == 1) {
    float4 ssr = GetSSRTexture().SampleLevel(LINEAR_CLAMPED_SAMPLER, screenCoords, 0);
    lighting.indirect.specular = lerp(lighting.indirect.specular, ssr.rgb * surface.F, ssr.a);
  }
#endif

  ApplyLighting(surface, lighting, color);

  color = clamp(color, 0, 65000);

  return color;

#if 0
  // Specular anti-aliasing
  {
    const float strength = 1.0f;
    const float maxRoughnessGain = 0.02f;
    float roughness2 = roughness * roughness;
    float3 dndu = ddx(pixelData.N);
    float3 dndv = ddy(pixelData.N);
    float variance = (dot(dndu, dndu) + dot(dndv, dndv));
    float kernelRoughness2 = min(variance * strength, maxRoughnessGain);
    float filteredRoughness2 = saturate(roughness2 + kernelRoughness2);
    pixelData.Roughness = sqrt(filteredRoughness2);
  }
#endif
}
