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
    vec3 dfg;
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
    if (!mat.UseNormal)
    return normalize(inNormal);

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

void GetEnergyCompensationPixelData(inout PixelData pixelData) {
    pixelData.dfg = vec3(1.0); // TODO: PrefilteredDFG(pixel.perceptualRoughness, shading_NoV);
    pixelData.EnergyCompensation = 1.0 + pixelData.F0 * (1.0 / pixelData.dfg.y - 1.0);
}

vec3 IsotropicLobe(PixelData material, const vec3 h, float NoV, float NoL, float NoH, float LoH) {
    float D = distribution(material.Roughness, NoH, material.Normal, h);
    float V = visibility(material.Roughness, NoV, NoL);
    vec3 F = fresnel(material.F0, LoH);
    return (D * V) * F;
}

vec3 DiffuseLobe(PixelData material, float NoV, float NoL, float LoH) {
    float diff = Diffuse(material.Roughness, NoV, NoL, LoH);
    return material.Albedo.rgb * diff;
}

vec3 SpecularLobe(PixelData material, const vec3 h, float NoV, float NoL, float NoH, float LoH) {
    vec3 spec = IsotropicLobe(material, h, NoV, NoL, NoH, LoH);
    return spec;
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

vec3 Lighting(vec3 F0, vec3 wsPos, PixelData pixelData) {
    vec3 result = vec3(0);

    for (int i = 0; i < NumLights; i++) {
        Light currentLight = lights[i];

        float visibility = 1.0;

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
        } else if (currentLight.RotationType.w == DIRECTIONAL_LIGHT && saturate(dot(pixelData.Normal, lightDirection)) > 0.0) {
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
        }

        vec3 Lh = normalize(lightDirection + pixelData.View);

        float lightNoL = saturate(dot(pixelData.Normal, lightDirection));
        vec3 h = normalize(pixelData.View + lightDirection);

        float shading_NoV = clampNoV(dot(pixelData.Normal, pixelData.View));
        float NoV = shading_NoV;
        float NoL = saturate(lightNoL);
        float NoH = saturate(dot(pixelData.Normal, h));
        float LoH = saturate(dot(lightDirection, h));

        vec3 Fd = DiffuseLobe(pixelData, NoV, NoL, LoH);
        vec3 Fr = SpecularLobe(pixelData, h, NoV, NoL, NoH, LoH);

        vec3 color = Fd + Fr * pixelData.EnergyCompensation;

        vec3 sun_radiance = color * max(0, dot(lightDirection, pixelData.Normal));
        vec3 transmittance_lut = SampleLUT(u_TransmittanceLut, PlanetRadius, lightNoL, PlanetRadius, 0.0);

        //result += currentLight.Position.w * (value * sun_radiance * ComputeMicroShadowing(NoL, material.AO)) + transmittance_lut;

        visibility *= ComputeMicroShadowing(NoL, pixelData.AO);

        result += (color * currentLight.ColorRadius.rgb * currentLight.PositionIntensity.w) * (NoL * visibility);
    }

    return result;
}

vec3 EvaluateIBL(PixelData pixelData) {
    vec3 color = vec3(0.0);
    
    vec3 Fr = vec3(0.0);
    
    
    
    return color;
}

void main() {
    Material mat = Materials[MaterialIndex];
    vec2 scaledUV = inUV;
    scaledUV *= mat.UVScale;

    float perceptualRoughness;
    vec4 baseColor;

    if (mat.UseAlbedo) {
        baseColor = SRGBtoLINEAR(texture(albedoMap, scaledUV)) * mat.Color;
    } else {
        baseColor = mat.Color;
    }
    if (baseColor.a < mat.AlphaCutoff && mat.AlphaMode == ALPHA_MODE_MASK) {
        discard;
    }

    const float c_MinRoughness = 0.04;
    perceptualRoughness = mat.Roughness;
    float metallic = mat.Metallic;
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
    pixelData.Metallic = metallic;
    pixelData.Roughness = perceptualRoughnessToRoughness(perceptualRoughness);
    pixelData.PerceptualRoughness = perceptualRoughness;
    pixelData.Reflectance = mat.Reflectance;
    pixelData.Emissive = emmisive;
    pixelData.Normal = GetNormal(mat, scaledUV);
    pixelData.AO = ao;
    pixelData.View = normalize(CamPos.xyz - inWorldPos);
    pixelData.NDotV = max(dot(pixelData.Normal, pixelData.View), 1e-4);
    float reflectance = computeDielectricF0(mat.Reflectance);
    pixelData.F0 = computeF0(pixelData.Albedo, pixelData.Metallic.x, reflectance);

    GetEnergyCompensationPixelData(pixelData);
    
    // Specular anti-aliasing
    {
        const float strength = 1.0f;
        const float maxRoughnessGain = 0.02f;
        float roughness2 = perceptualRoughness * perceptualRoughness;
        vec3 dndu = dFdx(pixelData.Normal);
        vec3 dndv = dFdy(pixelData.Normal);
        float variance = (dot(dndu, dndu) + dot(dndv, dndv));
        float kernelRoughness2 = min(variance * strength, maxRoughnessGain);
        float filteredRoughness2 = saturate(roughness2 + kernelRoughness2);
        pixelData.Roughness = sqrt(filteredRoughness2);
    }

    // Apply GTAO
    if (EnableGTAO == 1) {
        vec2 uv = gl_FragCoord.xy / vec2(ScreenDimensions.x, ScreenDimensions.y);
        float aoVisibility = 1.0;
        uint value = texture(u_GTAO, uv).r;
        aoVisibility = value / 255.0;
        pixelData.Albedo *= aoVisibility;
    }

    float shadowDistance = 500;
    float transitionDistance = 40;

    vec4 viewPos = View * vec4(inWorldPos, 1.0);

    // vec3 Lr = 2.0 * material.NDotV * material.Normal - material.View; for IBL
    vec3 lightContribution = Lighting(pixelData.F0, inWorldPos, pixelData);

    vec3 finalColor = lightContribution + pixelData.Emissive;

    outColor = vec4(finalColor, pixelData.Albedo.a);
}