#include "Globals.hlsli"
#include "PBRCommon.hlsli"

[[vk::push_constant]] struct PushConstant {
  float4x4 ModelMatrix;
  uint64_t VertexBufferPtr;
  uint32_t MaterialIndex;
  float _pad;
} PushConst;

struct VSLayout {
  float4 Position : POSITION;
  float3 WorldPos : WORLD_POSITION;
  float3 Normal : NORMAL;
  float2 UV : TEXCOORD;
  float3 ViewPos : VIEW_POS;
};

VSLayout VSmain(uint vertexIndex : SV_VertexID) {
  const float3 vertexPosition = vk::RawBufferLoad<float3>(PushConst.VertexBufferPtr + vertexIndex * sizeof(Vertex));
  const float3 vertexNormal = vk::RawBufferLoad<float3>(PushConst.VertexBufferPtr + vertexIndex * sizeof(Vertex) + 16);
  const float2 vertexUV = vk::RawBufferLoad<float2>(PushConst.VertexBufferPtr + vertexIndex * sizeof(Vertex) + 32);

  const float4 locPos = mul(PushConst.ModelMatrix, float4(vertexPosition, 1.0));

  VSLayout output;
  output.Normal = normalize(mul(transpose((float3x3)PushConst.ModelMatrix), vertexNormal));
  output.WorldPos = locPos.xyz / locPos.w;
  output.ViewPos = mul(Camera.ViewMatrix, float4(locPos.xyz, 1.0)).xyz;
  output.UV = vertexUV;
  output.Position = mul(Camera.ProjectionMatrix * Camera.ViewMatrix, float4(output.WorldPos, 1.0));

  return output;
}

struct PixelData {
  float4 Albedo;
  float3 DiffuseColor;
  float Metallic;
  float Roughness;
  float PerceptualRoughness;
  float Reflectance;
  float3 Emissive;
  float3 Normal;
  float AO;
  float3 View;
  float NDotV;
  float3 F0;
  float3 EnergyCompensation;
  float2 dfg;
  float3 TransmittanceLut;
  float3 SpecularColor;
  float2 PixelPosition;
};

float3 GetNormal(VSLayout vs, Material mat, float2 uv) {
  if (!mat.UseNormal) {
    return normalize(vs.Normal);
  }

  const float3 tangentNormal = normalize(GetMaterialNormalTexture(PushConst.MaterialIndex).Sample(LINEAR_REPEATED_SAMPLER, uv).rgb * 2.0 - 1.0);

  float3 q1 = ddx(vs.WorldPos);
  float3 q2 = ddy(vs.WorldPos);
  float2 st1 = ddx(uv);
  float2 st2 = ddy(uv);

  float3 N = normalize(vs.Normal);
  float3 T = normalize(q1 * st2.y - q2 * st1.y);
  float3 B = -normalize(cross(N, T));
  float3x3 TBN = float3x3(T, B, N);

  return normalize(mul(TBN, tangentNormal));
}

static const float4x4 BiasMatrix = float4x4(0.5, 0.0, 0.0, 0.0, 0.0, 0.5, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.5, 0.5, 0.0, 1.0);

float4 GetShadowPosition(float3 wsPos, int cascadeIndex) {
  float4 shadowCoord = mul(BiasMatrix * GetScene().CascadeViewProjections[cascadeIndex], float4(wsPos, 1.0));
  return shadowCoord;
}

float GetShadowBias(const float3 lightDirection, const float3 normal) {
  float minBias = 0.0023f;
  float bias = max(minBias * (1.0 - dot(normal, lightDirection)), minBias);
  return bias;
}

float3 DiffuseLobe(PixelData pixelData, float NoV, float NoL, float LoH) {
  return pixelData.DiffuseColor.rgb * Diffuse(pixelData.Roughness, NoV, NoL, LoH);
}

float GeometricOcclusion(PixelData pixelData, float NoL) {
  float NdotL = NoL;
  float NdotV = pixelData.NDotV;
  float r = pixelData.Roughness * pixelData.Roughness;

  float attenuationL = 2.0 * NdotL / (NdotL + sqrt(r * r + (1.0 - r * r) * (NdotL * NdotL)));
  float attenuationV = 2.0 * NdotV / (NdotV + sqrt(r * r + (1.0 - r * r) * (NdotV * NdotV)));
  return attenuationL * attenuationV;
}

float3 SpecularReflection(float3 reflectance0, float3 reflectance90, float VoH) {
  return reflectance0 + (reflectance90 - reflectance0) * pow(clamp(1.0 - VoH, 0.0, 1.0), 5.0);
}

float MicrofacetDistribution(float alphaRoughness, float NoH) {
  float roughnessSq = alphaRoughness * alphaRoughness;
  float f = (NoH * roughnessSq - NoH) * NoH + 1.0;
  return roughnessSq / (PI * f * f);
}

#ifdef CUBEMAP_PIPELINE
float3 SpecularDFG(const PixelData pixel) {
  return lerp(pixel.dfg.xxx, pixel.dfg.yyy, pixel.F0);
}

float PerceptualRoughnessToLod(float perceptualRoughness) {
  return 4 * perceptualRoughness * (2.0 - perceptualRoughness);
}

float3 PrefilteredRadiance(const float3 r, float perceptualRoughness) {
  float lod = PerceptualRoughnessToLod(perceptualRoughness);
  return GetPrefilteredMapTexture().SampleLevel(LINEAR_CLAMPED_SAMPLER, r, lod).rgb;
}

float3 DiffuseIrradiance(PixelData pixel) {
  return GetIrradianceTexture().Sample(LINEAR_CLAMPED_SAMPLER, pixel.Normal).rgb;
}

float3 GetReflectedVector(PixelData pixel) {
  return 2.0 * pixel.NDotV * pixel.Normal - pixel.View;
}

float2 PrefilteredDFG(float perceptualRoughness, float NoV) {
  // TODO: Lod shouldn't be 0.0
  return GetBRDFLUTTexture().SampleLevel(LINEAR_CLAMPED_SAMPLER, float2(NoV, perceptualRoughness), 0.0).rg;
}

void GetEnergyCompensationPixelData(inout PixelData pixelData) {
  pixelData.dfg = PrefilteredDFG(pixelData.PerceptualRoughness, pixelData.NDotV);
  pixelData.EnergyCompensation = 1.0 + pixelData.F0 * (1.0 / max(0.01, pixelData.dfg.y) - 1.0);
}

#endif

float3 Lighting(VSLayout vsLayout, inout PixelData pixelData) {
  float3 result = float3(0.0, 0.0, 0.0);

  for (int i = 0; i < GetScene().NumLights; i++) {
    Light currentLight = Lights[i].Load<Light>(0);

    float visibility = 0.0;

    float3 lightDirection = currentLight.RotationType.xyz;
    lightDirection.z = 0;

    if (currentLight.RotationType.w == POINT_LIGHT) {
      // Vector to light
      float3 L = currentLight.PositionIntensity.xyz - vsLayout.WorldPos;
      // Distance from light to fragment position
      float dist = length(L);

      // Light to fragment
      L = normalize(L);

      float radius = currentLight.ColorRadius.w;

      // Attenuation
      float atten = radius / (pow(dist, 2.0) + 1.0);
      float attenuation = clamp(1.0 - (dist * dist) / (radius * radius), 0.0, 1.0);

      visibility = attenuation;

      lightDirection = L;
    }
    else if (currentLight.RotationType.w == SPOT_LIGHT) {
      float3 L = currentLight.PositionIntensity.xyz - vsLayout.WorldPos;
      float cutoffAngle = 0.5f; //- light.angle;
      float dist = length(L);
      L = normalize(L);
      float theta = dot(L.xyz, currentLight.RotationType.xyz);
      float epsilon = cutoffAngle - cutoffAngle * 0.9f;
      float attenuation = ((theta - cutoffAngle) / epsilon);              // atteunate when approaching the outer cone
      attenuation *= currentLight.ColorRadius.w / (pow(dist, 2.0) + 1.0); // saturate(1.0f - dist / light.range);

      visibility = clamp(attenuation, 0.0, 1.0);
    }
    else if (currentLight.RotationType.w == DIRECTIONAL_LIGHT) {
      int cascadeIndex = GetCascadeIndex(GetScene().CascadeSplits, vsLayout.ViewPos, SHADOW_MAP_CASCADE_COUNT);
      float4 shadowPosition = GetShadowPosition(vsLayout.WorldPos, cascadeIndex);
      float shadowBias = GetShadowBias(lightDirection, pixelData.Normal);
      visibility = 1.0 - Shadow(pixelData.PixelPosition,
                                true,
                                GetSunShadowArrayTexture(),
                                cascadeIndex,
                                shadowPosition,
                                GetScene().ScissorNormalized,
                                shadowBias);
      // shadow far attenuation   
      float3 v = vsLayout.WorldPos - Camera.Position;

      float z = dot(transpose(Camera.ViewMatrix)[2].xyz, v);

      const float shadowFar = 100.0;
      // shadowFarAttenuation
      float2 p = shadowFar > 0.0f ? 0.5f * float2(10.0, 10.0 / (shadowFar * shadowFar)) : float2(1.0, 0.0);
      visibility = 1.0 - ((1.0 - visibility) * saturate(p.x - z * z * p.y));

      // handle bright sun like this for now
      currentLight.PositionIntensity.w *= 0.2;
    }

    float lightNoL = saturate(dot(pixelData.Normal, lightDirection));
    float3 h = normalize(pixelData.View + lightDirection);

    float NoL = saturate(lightNoL);
    float NoH = saturate(dot(pixelData.Normal, h));
    float LoH = saturate(dot(lightDirection, h));
    float VoH = saturate(dot(pixelData.View, h));

    float3 specularColor = lerp(pixelData.F0, pixelData.Albedo.rgb, pixelData.Metallic);

    pixelData.SpecularColor = specularColor;

    // Compute reflectance.
    float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);

    float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);
    float3 specularEnvironmentR0 = specularColor.rgb;
    float3 specularEnvironmentR90 = float3(1.0, 1.0, 1.0) * reflectance90;

    float3 F = SpecularReflection(specularEnvironmentR0, specularEnvironmentR90, VoH); //SpecularLobe(pixelData, h, pixelData.NDotV, NoL, NoH, VoH);
    float D = MicrofacetDistribution(pixelData.Roughness, NoH);
    float G = GeometricOcclusion(pixelData, NoL);

    // Calculation of analytical lighting contribution
    float3 diffuseContrib = (1.0 - F) * DiffuseLobe(pixelData, pixelData.NDotV, NoL, LoH);
    float3 specContrib = F * D * G;
    float3 color = (diffuseContrib + specContrib);

#ifdef CUBEMAP_PIPELINE
    //color *= pixelData.EnergyCompensation;
#endif

    result += color * (NoL * visibility * ComputeMicroShadowing(NoL, pixelData.AO)) * (currentLight.ColorRadius.rgb * currentLight.PositionIntensity.w);

#ifndef CUBEMAP_PIPELINE
    float sun_radiance = max(0, dot(lightDirection, pixelData.Normal));
    float light_nol = saturate(dot(pixelData.Normal, lightDirection));
    //@TODO
    //float3 transmittance_lut = SampleLUT(u_TransmittanceLut, PlanetRadius, light_nol, 0.0, PlanetRadius);
    //pixelData.TransmittanceLut = transmittance_lut;
    //result *= transmittance_lut;
#endif
    //result += currentLight.Position.w * (value * sun_radiance * ComputeMicroShadowing(NoL, material.AO)) + transmittance_lut;
  }

  return result;
}

float3 EvaluateIBL(PixelData pixel) {
#ifdef CUBEMAP_PIPELINE
    float3 E = SpecularDFG(pixel);
    float3 r = GetReflectedVector(pixel);
    float3 Fr = E * PrefilteredRadiance(r, pixel.PerceptualRoughness) * pixel.SpecularColor;

    float3 diffuseIrradiance = DiffuseIrradiance(pixel);

    float3 Fd = pixel.DiffuseColor.xyz * diffuseIrradiance;

    return Fd + Fr;
#endif

  return float3(0.0, 0.0, 0.0);
}

float4 PSmain(VSLayout input, float4 position : SV_Position) : SV_Target {
  Material mat = Materials[PushConst.MaterialIndex].Load<Material>(0);
  float2 scaledUV = input.UV;
  scaledUV *= mat.UVScale;

  float4 baseColor;

  if (mat.UseAlbedo) {
    baseColor = MaterialTextureMaps[ALBEDO_MAP_INDEX].Sample(LINEAR_REPEATED_SAMPLER, scaledUV) * mat.Color;
  }
  else {
    baseColor = mat.Color;
  }
  if (baseColor.a < mat.AlphaCutoff && mat.AlphaMode == ALPHA_MODE_MASK) {
    discard;
  }

  float roughness = mat.Roughness;
  float metallic = mat.Metallic;

  // metallic roughness workflow
  if (mat.UsePhysicalMap) {
    float4 physicalMapTexture = MaterialTextureMaps[PHYSICAL_MAP_INDEX].Sample(LINEAR_REPEATED_SAMPLER, scaledUV);
    roughness = physicalMapTexture.g; //* (mat.Roughness);
    metallic = physicalMapTexture.b;  // * (mat.Metallic);
  }
  else {
    metallic = clamp(metallic, 0.0, 1.0);
    roughness = clamp(roughness, MIN_ROUGHNESS, 1.0);
  }

  const float emissiveFactor = 1.0f;
  float3 emmisive = float3(0.0, 0.0, 0.0);
  if (mat.UseEmissive) {
    float3 value = MaterialTextureMaps[EMISSIVE_MAP_INDEX].Sample(LINEAR_REPEATED_SAMPLER, scaledUV).rgb * emissiveFactor;
    emmisive += value;
  }
  else {
    emmisive += mat.Emissive.rgb * mat.Emissive.a;
  }

  float ao = mat.UseAO ? MaterialTextureMaps[AO_MAP_INDEX].Sample(LINEAR_REPEATED_SAMPLER, scaledUV).r : 1.0;

  PixelData pixelData;
  pixelData.Albedo = baseColor;
  pixelData.DiffuseColor = ComputeDiffuseColor(baseColor, metallic);
  pixelData.Metallic = metallic;
  pixelData.PerceptualRoughness = clamp(roughness, MIN_PERCEPTUAL_ROUGHNESS, 1.0);
  pixelData.Roughness = PerceptualRoughnessToRoughness(pixelData.Roughness);
  pixelData.Reflectance = mat.Reflectance;
  pixelData.Emissive = emmisive;
  pixelData.Normal = GetNormal(input, mat, scaledUV);
  pixelData.AO = ao;
  pixelData.View = normalize(Camera.Position.xyz - input.WorldPos);
  pixelData.NDotV = clampNoV(dot(pixelData.Normal, pixelData.View));
  float reflectance = computeDielectricF0(mat.Reflectance);
  pixelData.F0 = computeF0(pixelData.Albedo, pixelData.Metallic, reflectance);
  pixelData.PixelPosition = position.xy;

  // Specular anti-aliasing
  {
    const float strength = 1.0f;
    const float maxRoughnessGain = 0.02f;
    float roughness2 = roughness * roughness;
    float3 dndu = ddx(pixelData.Normal);
    float3 dndv = ddy(pixelData.Normal);
    float variance = (dot(dndu, dndu) + dot(dndv, dndv));
    float kernelRoughness2 = min(variance * strength, maxRoughnessGain);
    float filteredRoughness2 = saturate(roughness2 + kernelRoughness2);
    pixelData.Roughness = sqrt(filteredRoughness2);
  }

#ifdef CUBEMAP_PIPELINE
    GetEnergyCompensationPixelData(pixelData);
#endif

  float3 lightContribution = Lighting(input, pixelData);
  float3 iblContribution = EvaluateIBL(pixelData);

  float3 finalColor = iblContribution + lightContribution + pixelData.Emissive;

#ifndef BLEND_MODE
  // Apply GTAO
  if (GetScene().EnableGTAO == 1) {
    float2 uv = position.xy / float2(GetScene().ScreenSize.x, GetScene().ScreenSize.y);
    float aoVisibility = 1.0;
    uint value = GetGTAOTexture().Sample(NEAREST_REPEATED_SAMPLER, uv).r;
    aoVisibility = value / 255.0;
    // maybe apply it to only pixelData.Albedo? 
    finalColor *= aoVisibility;
  }
#endif

  return float4(finalColor, pixelData.Albedo.a);
}
