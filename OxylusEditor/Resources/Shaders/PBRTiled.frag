#version 450
#pragma shader_stage(fragment)

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "PBR.glsl"
#include "Shadows.glsl"

#define PI 3.1415926535897932384626433832795

#define NUM_PCF_SAMPLES 8

#define DIRECTIONAL_LIGHT 0
#define POINT_LIGHT 1
#define SPOT_LIGHT 2

struct Light {
    vec4 PositionIntensity;// w: intensity
    vec4 ColorRadius;// w: radius
    vec4 RotationType;// w: type
};

struct PixelData {
    vec4 Albedo;
    vec3 DiffuseColor;
    float Metallic;
    float Roughness;
    float PerceptualRoughness;
    float Reflectance;
    vec3 Emissive;
    vec3 Normal;
    float AO;
    vec3 View;
    float NDotV;
    vec3 F0;
    vec3 EnergyCompensation;
    vec2 dfg;
    vec3 TransmittanceLut;
    vec3 SpecularColor;
};

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 in_ViewPos;

layout(binding = 0) uniform UBO {
    mat4 Projection;
    mat4 View;
    vec3 CamPos;
    float PlanetRadius;
};

layout(binding = 1) uniform UBOParams {
    ivec2 ScreenDimensions;
    int NumLights;
    int EnableGTAO;
};

layout(binding = 2) buffer Lights { Light lights[]; };

// Shadows
layout(binding = 3) uniform sampler2DArray in_DirectShadows;

#define SHADOW_MAP_CASCADE_COUNT 4

layout(binding = 4) uniform ShadowUBO {
    mat4 CascadeViewProjMat[SHADOW_MAP_CASCADE_COUNT];
    vec4 CascadeSplits;
    vec4 ScissorNormalized;
};

layout(binding = 5) uniform sampler2D u_TransmittanceLut;
layout(binding = 6) uniform usampler2D u_GTAO;

#ifdef CUBEMAP_PIPELINE
layout(binding = 7) uniform texture2D u_BRDFLut;
layout(binding = 8) uniform textureCube u_PrefilteredMap;
layout(binding = 9) uniform textureCube u_Irradiance;
#endif

layout(binding = 10) uniform sampler u_Sampler;

#include "SkyCommon.glsl"

// Material
layout(set = 1, binding = 0) uniform sampler2D albedoMap;
layout(set = 1, binding = 1) uniform sampler2D normalMap;
layout(set = 1, binding = 2) uniform sampler2D aoMap;
layout(set = 1, binding = 3) uniform sampler2D physicalMap;
layout(set = 1, binding = 4) uniform sampler2D emissiveMap;

layout(location = 0) out vec4 outColor;

#include "Material.glsl"
// #define MANUAL_SRGB 1 // we have to tonemap some inputs to make this viable
#include "Conversions.glsl"

vec3 GetNormal(Material mat, vec2 uv) {
    if (!mat.UseNormal) {
        return normalize(inNormal);
    }

    vec3 tangentNormal = normalize(texture(normalMap, uv).xyz * 2.0 - 1.0);

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

mat4 BiasMatrix = mat4(0.5, 0.0, 0.0, 0.0, 0.0, 0.5, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.5, 0.5, 0.0, 1.0);

vec4 GetShadowPosition(vec3 wsPos, int cascadeIndex) {
    vec4 shadowCoord = BiasMatrix * CascadeViewProjMat[cascadeIndex] * vec4(wsPos, 1.0);
    return shadowCoord;
}

float GetShadowBias(const vec3 lightDirection, const vec3 normal) {
    float minBias = 0.0023f;
    float bias = max(minBias * (1.0 - dot(normal, lightDirection)), minBias);
    return bias;
}

vec3 DiffuseLobe(PixelData pixelData, float NoV, float NoL, float LoH) {
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

vec3 SpecularReflection(vec3 reflectance0, vec3 reflectance90, float VoH) {
    return reflectance0 + (reflectance90 - reflectance0) * pow(clamp(1.0 - VoH, 0.0, 1.0), 5.0);
}

float MicrofacetDistribution(float alphaRoughness, float NoH) {
    float roughnessSq = alphaRoughness * alphaRoughness;
    float f = (NoH * roughnessSq - NoH) * NoH + 1.0;
    return roughnessSq / (PI * f * f);
}

#ifdef CUBEMAP_PIPELINE
vec3 SpecularDFG(const PixelData pixel) {
    return mix(pixel.dfg.xxx, pixel.dfg.yyy, pixel.F0);
}

float PerceptualRoughnessToLod(float perceptualRoughness) {
    return 4 * perceptualRoughness * (2.0 - perceptualRoughness);
}

vec3 PrefilteredRadiance(const vec3 r, float perceptualRoughness) {
    float lod = PerceptualRoughnessToLod(perceptualRoughness);
    return textureLod(samplerCube(u_PrefilteredMap, u_Sampler), r, lod).rgb;
}

vec3 DiffuseIrradiance(PixelData pixel) {
    return texture(samplerCube(u_Irradiance, u_Sampler), pixel.Normal).rgb;
}

vec3 GetReflectedVector(PixelData pixel) {
    return 2.0 * pixel.NDotV * pixel.Normal - pixel.View;
}

vec2 PrefilteredDFG(float perceptualRoughness, float NoV) {
    return textureLod(sampler2D(u_BRDFLut, u_Sampler), vec2(NoV, perceptualRoughness), 0.0).rg;
}

void GetEnergyCompensationPixelData(inout PixelData pixelData) {
    pixelData.dfg = PrefilteredDFG(pixelData.PerceptualRoughness, pixelData.NDotV);
    pixelData.EnergyCompensation = 1.0 + pixelData.F0 * (1.0 / max(0.01, pixelData.dfg.y) - 1.0);
}
#endif

vec3 Lighting(vec3 F0, vec3 wsPos, inout PixelData pixelData) {
    vec3 result = vec3(0);

    for (int i = 0; i < NumLights; i++) {
        Light currentLight = lights[i];

        float visibility = 0.0;

        vec3 lightDirection = currentLight.RotationType.xyz;
        lightDirection.z = 0;

        if (currentLight.RotationType.w == POINT_LIGHT) {
            // Vector to light
            vec3 L = currentLight.PositionIntensity.xyz - inWorldPos;
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
        } else if (currentLight.RotationType.w == SPOT_LIGHT) {
            vec3 L = currentLight.PositionIntensity.xyz - wsPos;
            float cutoffAngle = 0.5f;//- light.angle;
            float dist = length(L);
            L = normalize(L);
            float theta = dot(L.xyz, currentLight.RotationType.xyz);
            float epsilon = cutoffAngle - cutoffAngle * 0.9f;
            float attenuation = ((theta - cutoffAngle) / epsilon);// atteunate when approaching the outer cone
            attenuation *= currentLight.ColorRadius.w / (pow(dist, 2.0) + 1.0);// saturate(1.0f - dist / light.range);

            visibility = clamp(attenuation, 0.0, 1.0);
        } else if (currentLight.RotationType.w == DIRECTIONAL_LIGHT) {
            int cascadeIndex = GetCascadeIndex(CascadeSplits, in_ViewPos, SHADOW_MAP_CASCADE_COUNT);
            highp vec4 shadowPosition = GetShadowPosition(inWorldPos, cascadeIndex);
            float shadowBias = GetShadowBias(lightDirection, pixelData.Normal);
            visibility = 1.0 - Shadow(true, in_DirectShadows, cascadeIndex, shadowPosition, ScissorNormalized, shadowBias);
            // shadow far attenuation   
            highp vec3 v = inWorldPos - CamPos;
            highp float z = dot(transpose(View)[2].xyz, v);

            const float shadowFar = 100.0;
            // shadowFarAttenuation
            highp vec2 p = shadowFar > 0.0f ? 0.5f * vec2(10.0, 10.0 / (shadowFar * shadowFar)) : vec2(1.0, 0.0);
            visibility = 1.0 - ((1.0 - visibility) * saturate(p.x - z * z * p.y));

            // handle bright sun like this for now
            currentLight.PositionIntensity.w *= 0.2;
        }

        float lightNoL = saturate(dot(pixelData.Normal, lightDirection));
        vec3 h = normalize(pixelData.View + lightDirection);

        float NoL = saturate(lightNoL);
        float NoH = saturate(dot(pixelData.Normal, h));
        float LoH = saturate(dot(lightDirection, h));
        float VoH = saturate(dot(pixelData.View, h));

        vec3 specularColor = mix(pixelData.F0, pixelData.Albedo.rgb, pixelData.Metallic);

        pixelData.SpecularColor = specularColor;

        // Compute reflectance.
        float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);

        float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);
        vec3 specularEnvironmentR0 = specularColor.rgb;
        vec3 specularEnvironmentR90 = vec3(1.0, 1.0, 1.0) * reflectance90;

        vec3 F = SpecularReflection(specularEnvironmentR0, specularEnvironmentR90, VoH); //SpecularLobe(pixelData, h, pixelData.NDotV, NoL, NoH, VoH);
        float D = MicrofacetDistribution(pixelData.Roughness, NoH);
        float G = GeometricOcclusion(pixelData, NoL);
        
        // Calculation of analytical lighting contribution
        vec3 diffuseContrib = (1.0 - F) * DiffuseLobe(pixelData, pixelData.NDotV, NoL, LoH);
        vec3 specContrib = F * D * G;
        vec3 color = (diffuseContrib + specContrib);

#ifdef CUBEMAP_PIPELINE
        //color *= pixelData.EnergyCompensation;
#endif

        result += color * (NoL * visibility * ComputeMicroShadowing(NoL, pixelData.AO)) * (currentLight.ColorRadius.rgb * currentLight.PositionIntensity.w);

#ifndef CUBEMAP_PIPELINE
        float sun_radiance = max(0, dot(lightDirection, pixelData.Normal));
        float light_nol = saturate(dot(pixelData.Normal, lightDirection));
        vec3 transmittance_lut = SampleLUT(u_TransmittanceLut, PlanetRadius, light_nol, 0.0, PlanetRadius);
        pixelData.TransmittanceLut = transmittance_lut;
        result *= transmittance_lut;
#endif
        //result += currentLight.Position.w * (value * sun_radiance * ComputeMicroShadowing(NoL, material.AO)) + transmittance_lut;
    }

    return result;
}

vec3 EvaluateIBL(PixelData pixel) {
#ifdef CUBEMAP_PIPELINE
    vec3 E = SpecularDFG(pixel);
    vec3 r = GetReflectedVector(pixel);
    vec3 Fr = E * PrefilteredRadiance(r, pixel.PerceptualRoughness) * pixel.SpecularColor;

    vec3 diffuseIrradiance = DiffuseIrradiance(pixel);

    vec3 Fd = pixel.DiffuseColor.xyz * diffuseIrradiance;

    return Fd + Fr;
#endif

    return vec3(0);
}

void main() {
    Material mat = Materials[MaterialIndex];
    vec2 scaledUV = inUV;
    scaledUV *= mat.UVScale;

    vec4 baseColor;

    if (mat.UseAlbedo) {
        baseColor = SRGBtoLINEAR(texture(albedoMap, scaledUV)) * mat.Color;
    } else {
        baseColor = mat.Color;
    }
    if (baseColor.a < mat.AlphaCutoff && mat.AlphaMode == ALPHA_MODE_MASK) {
        discard;
    }

    float roughness = mat.Roughness;
    float metallic = mat.Metallic;

    // metallic roughness workflow
    if (mat.UsePhysicalMap) {
        vec4 physicalMapTexture = texture(physicalMap, scaledUV);
        roughness = physicalMapTexture.g;//* (mat.Roughness);
        metallic = physicalMapTexture.b;// * (mat.Metallic);
    } else {
        metallic = clamp(metallic, 0.0, 1.0);
        roughness = clamp(roughness, MIN_ROUGHNESS, 1.0);
    }

    const float u_EmissiveFactor = 1.0f;
    vec3 emmisive = vec3(0);
    if (mat.UseEmissive) {
        vec3 value = SRGBtoLINEAR(texture(emissiveMap, scaledUV)).rgb * u_EmissiveFactor;
        emmisive += value;
    } else {
        emmisive += mat.Emissive.rgb * mat.Emissive.a;
    }

    float ao = mat.UseAO ? texture(aoMap, scaledUV).r : 1.0;

    PixelData pixelData;
    pixelData.Albedo = baseColor;
    pixelData.DiffuseColor = ComputeDiffuseColor(baseColor, metallic);
    pixelData.Metallic = metallic;
    pixelData.PerceptualRoughness = clamp(roughness, MIN_PERCEPTUAL_ROUGHNESS, 1.0);
    pixelData.Roughness = perceptualRoughnessToRoughness(pixelData.Roughness);
    pixelData.Reflectance = mat.Reflectance;
    pixelData.Emissive = emmisive;
    pixelData.Normal = GetNormal(mat, scaledUV);
    pixelData.AO = ao;
    pixelData.View = normalize(CamPos.xyz - inWorldPos);
    pixelData.NDotV = clampNoV(dot(pixelData.Normal, pixelData.View));
    float reflectance = computeDielectricF0(mat.Reflectance);
    pixelData.F0 = computeF0(pixelData.Albedo, pixelData.Metallic, reflectance);

    // Specular anti-aliasing
    {
        const float strength = 1.0f;
        const float maxRoughnessGain = 0.02f;
        float roughness2 = roughness * roughness;
        vec3 dndu = dFdx(pixelData.Normal);
        vec3 dndv = dFdy(pixelData.Normal);
        float variance = (dot(dndu, dndu) + dot(dndv, dndv));
        float kernelRoughness2 = min(variance * strength, maxRoughnessGain);
        float filteredRoughness2 = saturate(roughness2 + kernelRoughness2);
        pixelData.Roughness = sqrt(filteredRoughness2);
    }
    
#ifdef CUBEMAP_PIPELINE
    GetEnergyCompensationPixelData(pixelData);
#endif

    vec3 lightContribution = Lighting(pixelData.F0, inWorldPos, pixelData);
    vec3 iblContribution = EvaluateIBL(pixelData);

    vec3 finalColor = iblContribution + lightContribution + pixelData.Emissive;

#ifndef BLEND_MODE
    // Apply GTAO
    if (EnableGTAO == 1) {
        vec2 uv = gl_FragCoord.xy / vec2(ScreenDimensions.x, ScreenDimensions.y);
        float aoVisibility = 1.0;
        uint value = texture(u_GTAO, uv).r;
        aoVisibility = value / 255.0;
        // maybe apply it to only pixelData.Albedo? 
        finalColor *= aoVisibility;
    }
#endif

    outColor = vec4(finalColor, pixelData.Albedo.a);
}