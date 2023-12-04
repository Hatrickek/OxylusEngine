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
    vec4 Position; // w: intensity
    vec4 Color;    // w: radius
    vec4 Rotation; // w: type
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

layout(binding = 4) uniform ShadowUBO {
    mat4 CascadeViewProjMat[SHADOW_MAP_CASCADE_COUNT];
    vec4 CascadeSplits;
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

float ShadowFade = 1.0;

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

vec2 VogelDiskSample(int sampleIndex, int samplesCount, float phi) {
    float GoldenAngle = 2.4f;

    float r = sqrt(sampleIndex + 0.5f) / sqrt(samplesCount);
    float theta = sampleIndex * GoldenAngle + phi;

    float sine = sin(theta);
    float cosine = cos(theta);

    return vec2(r * cosine, r * sine);
}

float GetShadowBias(vec3 lightDirection, vec3 normal, int shadowIndex) {
    float minBias = 0.0023f;
    float bias = max(minBias * (1.0 - dot(normal, lightDirection)), minBias);
    return bias;
}

float Noise(vec2 co) { return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453); }

float PCFShadowDirectionalLight(sampler2DArray shadowMap, vec4 shadowCoords, float uvRadius, vec3 lightDirection, vec3 normal, vec3 wsPos, int cascadeIndex) {
    float bias = GetShadowBias(lightDirection, normal, cascadeIndex);
    float sum = 0;
    float noise = Noise(wsPos.xy);

    for (int i = 0; i < NUM_PCF_SAMPLES; i++) {
        vec2 offset = VogelDiskSample(i, NUM_PCF_SAMPLES, noise) / 700.0f;

        float z = texture(shadowMap, vec3(shadowCoords.xy + offset, cascadeIndex)).r;
        sum += step(shadowCoords.z - bias, z);
    }

    return sum / NUM_PCF_SAMPLES;
}

mat4 BiasMatrix = mat4(0.5, 0.0, 0.0, 0.0, 0.0, 0.5, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.5, 0.5, 0.0, 1.0);

float CalculateShadow(vec3 wsPos, int cascadeIndex, vec3 lightDirection, vec3 normal) {
    vec4 shadowCoord = BiasMatrix * CascadeViewProjMat[cascadeIndex] * vec4(wsPos, 1.0);
    shadowCoord = shadowCoord * (1.0 / shadowCoord.w);
    float NEAR = 0.01;
    const float lightSize = 1.5;
    float uvRadius = lightSize * NEAR / shadowCoord.z;
    uvRadius = min(uvRadius, 0.002f);
    vec4 viewPos = View * vec4(wsPos, 1.0);

    float shadowAmount = PCFShadowDirectionalLight(in_DirectShadows, shadowCoord, uvRadius, lightDirection, normal, wsPos, cascadeIndex);

    const float CascadeFade = 3.0;

    float cascadeFade = smoothstep(CascadeSplits[cascadeIndex].x + CascadeFade, CascadeSplits[cascadeIndex].x, viewPos.z);
    int cascadeNext = cascadeIndex + 1;
    if (cascadeFade > 0.0 && cascadeNext < SHADOW_MAP_CASCADE_COUNT) {
        shadowCoord = BiasMatrix * CascadeViewProjMat[cascadeNext] * vec4(wsPos, 1.0);
        float shadowAmount1 = PCFShadowDirectionalLight(in_DirectShadows, shadowCoord, uvRadius, lightDirection, normal, wsPos, cascadeNext);

        shadowAmount = mix(shadowAmount, shadowAmount1, cascadeFade);
    }

    return 1.0 - ((1.0 - shadowAmount) * ShadowFade);
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

vec3 Lighting(vec3 F0, vec3 wsPos, PixelData material) {
    vec3 result = vec3(0);

    for (int i = 0; i < NumLights; i++) {
        Light currentLight = lights[i];

        float value = 0.0;

        if (currentLight.Rotation.w == POINT_LIGHT) {
            // Vector to light
            vec3 L = currentLight.Position.xyz - inWorldPos;
            // Distance from light to fragment position
            float dist = length(L);

            // Light to fragment
            L = normalize(L);

            float radius = currentLight.Color.w;

            // Attenuation
            float atten = radius / (pow(dist, 2.0) + 1.0);
            float attenuation = clamp(1.0 - (dist * dist) / (radius * radius), 0.0, 1.0);

            value = attenuation;

            currentLight.Rotation = vec4(L, 1.0);
        } else if (currentLight.Rotation.w == SPOT_LIGHT) {
            vec3 L = currentLight.Position.xyz - wsPos;
            float cutoffAngle = 0.5f; //- light.angle;
            float dist = length(L);
            L = normalize(L);
            float theta = dot(L.xyz, currentLight.Rotation.xyz);
            float epsilon = cutoffAngle - cutoffAngle * 0.9f;
            float attenuation = ((theta - cutoffAngle) / epsilon); // atteunate when approaching the outer cone
            attenuation *= currentLight.Color.w / (pow(dist, 2.0) + 1.0);  // saturate(1.0f - dist / light.range);

            value = clamp(attenuation, 0.0, 1.0);
        } else if (currentLight.Rotation.w == DIRECTIONAL_LIGHT) {
            int cascadeIndex = GetCascadeIndex(CascadeSplits, in_ViewPos, SHADOW_MAP_CASCADE_COUNT);
            value = CalculateShadow(wsPos, cascadeIndex, currentLight.Rotation.xyz, material.Normal);
        }

        vec3 Li = currentLight.Rotation.xyz;
        Li.z = 0;
        vec3 Lradiance = currentLight.Color.xyz * currentLight.Position.w; // intensity is w of position;
        vec3 Lh = normalize(Li + material.View);

        float lightNoL = saturate(dot(material.Normal, Li));
        vec3 h = normalize(material.View + Li);

        float shading_NoV = clampNoV(dot(material.Normal, material.View));
        float NoV = shading_NoV;
        float NoL = saturate(lightNoL);
        float NoH = saturate(dot(material.Normal, h));
        float LoH = saturate(dot(Li, h));

        vec3 Fd = DiffuseLobe(material, NoV, NoL, LoH);
        vec3 Fr = SpecularLobe(material, h, NoV, NoL, NoH, LoH);

        vec3 color = Fd + Fr;
        result += (color * Lradiance.rgb) * (value * NoL * ComputeMicroShadowing(NoL, material.AO));
    }

    return result;
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
        emmisive += vec3(mat.Emissive) * mat.Emissive.a;
    }

    float ao = mat.UseAO ? texture(aoMap, scaledUV).r : 1.0;

    PixelData material;
    material.Albedo = baseColor;
    material.Metallic = metallic;
    material.Roughness = perceptualRoughnessToRoughness(perceptualRoughness);
    material.PerceptualRoughness = perceptualRoughness;
    material.Reflectance = mat.Reflectance;
    material.Emissive = emmisive;
    material.Normal = GetNormal(mat, scaledUV);
    material.AO = ao;
    material.View = normalize(CamPos.xyz - inWorldPos);
    material.NDotV = max(dot(material.Normal, material.View), 1e-4);
    float reflectance = computeDielectricF0(mat.Reflectance);
    material.F0 = computeF0(material.Albedo, material.Metallic.x, reflectance);

    // Specular anti-aliasing
    {
        const float strength = 1.0f;
        const float maxRoughnessGain = 0.02f;
        float roughness2 = perceptualRoughness * perceptualRoughness;
        vec3 dndu = dFdx(material.Normal);
        vec3 dndv = dFdy(material.Normal);
        float variance = (dot(dndu, dndu) + dot(dndv, dndv));
        float kernelRoughness2 = min(variance * strength, maxRoughnessGain);
        float filteredRoughness2 = saturate(roughness2 + kernelRoughness2);
        material.Roughness = sqrt(filteredRoughness2);
    }

    // Apply GTAO
    if (EnableGTAO == 1) {
        vec2 uv = gl_FragCoord.xy / vec2(ScreenDimensions.x, ScreenDimensions.y);
        float aoVisibility = 1.0;
        uint value = texture(u_GTAO, uv).r;
        aoVisibility = value / 255.0;
        material.Albedo *= aoVisibility;
    }

    float shadowDistance = 500;
    float transitionDistance = 40;

    vec4 viewPos = View * vec4(inWorldPos, 1.0);

    float distance = length(viewPos);
    ShadowFade = distance - (shadowDistance - transitionDistance);
    ShadowFade /= transitionDistance;
    ShadowFade = clamp(1.0 - ShadowFade, 0.0, 1.0);

    // vec3 Lr = 2.0 * material.NDotV * material.Normal - material.View; for IBL
    vec3 lightContribution = Lighting(material.F0, inWorldPos, material);

    vec3 finalColor = lightContribution + material.Emissive;

    outColor = vec4(finalColor, material.Albedo.a);
}