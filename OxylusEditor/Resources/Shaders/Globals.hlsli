#ifndef GLOBALS_HLSLI
#define GLOBALS_HLSLI

#include "Common.hlsli"
#include "Materials.hlsli"

[[vk::binding(0, 0)]] ConstantBuffer<SceneData> Scene;
[[vk::binding(1, 0)]] ConstantBuffer<CameraData> Camera;

[[vk::binding(2, 0)]] ByteAddressBuffer Buffers[];

[[vk::binding(3, 0)]] Texture2D<float4> SceneTextures[];
[[vk::binding(4, 0)]] Texture2D<uint> SceneUintTextures[];
[[vk::binding(5, 0)]] TextureCube<float4> SceneCubeTextures[];
[[vk::binding(6, 0)]] Texture2DArray<float4> SceneArrayTextures[];
[[vk::binding(7, 0)]] RWTexture2D<float4> SceneRWTextures[];
[[vk::binding(8, 0)]] Texture2D<float4> MaterialTextureMaps[];

// 0 - Linear Clamped
// 1 - Linear Repeated
// 2 - Nearest Clamped
// 3 - Nearest Repeated
[[vk::binding(9, 0)]] SamplerState Samplers[4];

#define LINEAR_CLAMPED_SAMPLER Samplers[0]
#define LINEAR_REPEATED_SAMPLER Samplers[1]
#define NEAREST_CLAMPED_SAMPLER Samplers[2]
#define NEAREST_REPEATED_SAMPLER Samplers[3]

SceneData GetScene() { return Scene; }
CameraData GetCamera() { return Camera; }

// scene textures
Texture2D<float4> GetPBRTexture() { return SceneTextures[GetScene().Indices.PBRImageIndex]; }
Texture2D<float4> GetNormalTexture() { return SceneTextures[GetScene().Indices.NormalImageIndex]; }
Texture2D<float4> GetDepthTexture() { return SceneTextures[GetScene().Indices.DepthImageIndex]; }
Texture2D<float4> GetSkyTransmittanceLUTTexture() { return SceneTextures[GetScene().Indices.SkyTransmittanceLutIndex]; }
Texture2D<float4> GetSkyMultiScatterLUTTexture() { return SceneTextures[GetScene().Indices.SkyMultiscatterLutIndex]; }
//Texture2D<float4> GetSSRTexture() { return SceneTextures[GetScene().Indices.SSRBufferImageIndex]; }

// scene r/w textures
RWTexture2D<float4> GetSkyTransmittanceLUTRWTexture() { return SceneRWTextures[GetScene().Indices.SkyTransmittanceLutIndex]; }
RWTexture2D<float4> GetSkyMultiScatterLUTRWTexture() { return SceneRWTextures[GetScene().Indices.SkyMultiscatterLutIndex]; }
RWTexture2D<float4> GetSSRRWTexture() { return SceneRWTextures[GetScene().Indices.SSRBufferImageIndex]; }

// scene cube textures
TextureCube<float4> GetSkyEnvMapTexture() { return SceneCubeTextures[GetScene().Indices.SkyEnvMapIndex]; }

// scene uint textures
Texture2D<uint> GetGTAOTexture() { return SceneUintTextures[GetScene().Indices.GTAOBufferImageIndex]; }

// scene array textures
Texture2DArray<float4> GetSunShadowArrayTexture() { return SceneArrayTextures[GetScene().Indices.ShadowArrayIndex]; }

// material textures
Texture2D<float4> GetMaterialAlbedoTexture(Material material) { return MaterialTextureMaps[material.AlbedoMapID]; }
Texture2D<float4> GetMaterialNormalTexture(Material material) { return MaterialTextureMaps[material.NormalMapID]; }
Texture2D<float4> GetMaterialPhysicalTexture(Material material) { return MaterialTextureMaps[material.PhysicalMapID]; }
Texture2D<float4> GetMaterialAOTexture(Material material) { return MaterialTextureMaps[material.AOMapID]; }
Texture2D<float4> GetMaterialEmissiveTexture(Material material) { return MaterialTextureMaps[material.EmissiveMapID]; }

Material GetMaterial(int materialIndex) { return Buffers[GetScene().Indices.MaterialsBufferIndex].Load<Material>(materialIndex * sizeof(Material)); }
Light GetLight(int lightIndex) { return Buffers[GetScene().Indices.LightsBufferIndex].Load<Light>(lightIndex * sizeof(Light)); }
SSRData GetSSRData() { return Buffers[GetScene().Indices.SSRBufferImageIndex].Load<SSRData>(0); }

#endif
