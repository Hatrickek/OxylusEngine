#include "Common.hlsli"
#include "Materials.hlsli"

[[vk::binding(0, 0)]] cbuffer SceneBuffer {
  SceneData Scene;
}

[[vk::binding(1, 0)]] cbuffer CameraBuffer {
  CameraData Camera;
}

[[vk::binding(2, 0)]] ByteAddressBuffer Buffers[];

[[vk::binding(3, 0)]] Texture2D<float4> SceneTextureMaps[];
[[vk::binding(4, 0)]] Texture2D<uint> SceneTextureMapsUint[];
[[vk::binding(5, 0)]] TextureCube<float4> SceneTextureCubeMaps[];
[[vk::binding(6, 0)]] Texture2DArray<float4> SceneTextureArrayMaps[];
[[vk::binding(7, 0)]] Texture2D<float4> MaterialTextureMaps[];
[[vk::binding(8, 0)]] RWTexture2D<float4> SceneRWTextureMaps[];

// 0 - Linear Clamped
// 1 - Linear Repeated
// 2 - Nearest Clamped
// 3 - Nearest Repeated
[[vk::binding(0, 1)]] SamplerState Samplers[4];

#define LINEAR_CLAMPED_SAMPLER Samplers[0]
#define LINEAR_REPEATED_SAMPLER Samplers[1]
#define NEAREST_CLAMPED_SAMPLER Samplers[2]
#define NEAREST_REPEATED_SAMPLER Samplers[3]

SceneData GetScene() {
  return Scene;
}

Texture2D<float4> GetBRDFLUTTexture() {
  return SceneTextureMaps[GetScene().Indices.BRDFLUTIndex];
}

Texture2D<float4> GetSkyTransmittanceLUTTexture() {
  return SceneTextureMaps[GetScene().Indices.SkyTransmittanceLutIndex];
}

RWTexture2D<float4> GetSkyTransmittanceLUTRWTexture() {
  return SceneRWTextureMaps[GetScene().Indices.SkyTransmittanceLutIndex];
}

RWTexture2D<float4> GetSSRRWTexture() {
  return SceneRWTextureMaps[GetScene().Indices.SSRIndex];
}

Texture2D<float4> GetSkyMultiScatterLUTTexture() {
  return SceneTextureMaps[GetScene().Indices.SkyMultiscatterLutIndex];
}

TextureCube<float4> GetPrefilteredMapTexture() {
  return SceneTextureCubeMaps[GetScene().Indices.PrefilteredCubeMapIndex];
}

TextureCube<float4> GetIrradianceTexture() {
  return SceneTextureCubeMaps[GetScene().Indices.IrradianceMapIndex];
}

TextureCube<float4> GetCubeMapTexture() {
  return SceneTextureCubeMaps[GetScene().Indices.CubeMapIndex];
}

Texture2D<uint> GetGTAOTexture() {
  return SceneTextureMapsUint[GetScene().Indices.GTAOIndex];
}

Texture2DArray<float4> GetSunShadowArrayTexture() {
  return SceneTextureArrayMaps[GetScene().Indices.ShadowArrayIndex];
}

Texture2D<float4> GetMaterialAlbedoTexture(int materialIndex) {
  return MaterialTextureMaps[GetAlbedoTextureIndex(materialIndex)];
}

Texture2D<float4> GetMaterialNormalTexture(int materialIndex) {
  return MaterialTextureMaps[GetNormalTextureIndex(materialIndex)];
}

Texture2D<float4> GetMaterialPhysicalTexture(int materialIndex) {
  return MaterialTextureMaps[GetPhysicalTextureIndex(materialIndex)];
}

Texture2D<float4> GetMaterialAOTexture(int materialIndex) {
  return MaterialTextureMaps[GetAOTextureIndex(materialIndex)];
}

Texture2D<float4> GetMaterialEmissiveTexture(int materialIndex) {
  return MaterialTextureMaps[GetEmissiveTextureIndex(materialIndex)];
}

Material GetMaterial(int materialIndex) {
  return Buffers[Scene.Indices.MaterialsBufferIndex].Load<Material>(materialIndex * sizeof(Material));
}

Light GetLight(int lightIndex) {
  return Buffers[Scene.Indices.LightsBufferIndex].Load<Light>(lightIndex * sizeof(Light));
}

CameraData GetCamera() {
  return Camera;
}
