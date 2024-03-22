#ifndef GLOBALS_HLSLI
#define GLOBALS_HLSLI

#include "Common.hlsli"
#include "Materials.hlsli"

[[vk::binding(0, 1)]] ConstantBuffer<CameraCB> Camera;
[[vk::binding(1, 1)]] StructuredBuffer<MeshInstancePointer> MeshInstancePointers;
//[[vk::binding(1, 2)]] StructuredBuffer<DrawParameter> DrawParameters;

[[vk::binding(0, 0)]] ConstantBuffer<SceneData> Scene;

[[vk::binding(1, 0)]] ByteAddressBuffer Buffers[];

[[vk::binding(2, 0)]] Texture2D<float4> SceneTextures[];
[[vk::binding(3, 0)]] Texture2D<uint> SceneUintTextures[];
[[vk::binding(4, 0)]] TextureCube<float4> SceneCubeTextures[];
[[vk::binding(5, 0)]] Texture2DArray<float4> SceneArrayTextures[];
[[vk::binding(6, 0)]] RWTexture2D<float4> SceneRWTextures[];
[[vk::binding(7, 0)]] Texture2D<float4> MaterialTextureMaps[];

// 0 - Linear Clamped
// 1 - Linear Repeated
// 2 - Linear Repeated
// 3 - Nearest Clamped
// 4 - Nearest Repeated
[[vk::binding(8, 0)]] SamplerState Samplers[5];
[[vk::binding(9, 0)]] SamplerComparisonState ComparisonSamplers[5];

#define LINEAR_CLAMPED_SAMPLER Samplers[0]
#define LINEAR_REPEATED_SAMPLER Samplers[1]
#define LINEAR_REPEATED_SAMPLER_ANISOTROPY Samplers[2]
#define NEAREST_CLAMPED_SAMPLER Samplers[3]
#define NEAREST_REPEATED_SAMPLER Samplers[4]

#define CMP_DEPTH_SAMPLER ComparisonSamplers[0]

SceneData GetScene() { return Scene; }

CameraData GetCamera(uint camera_index = 0) { return Camera.camera_data[camera_index]; }

inline MeshInstance load_instance(uint instance_index) {
  return Buffers[GetScene().indices_.mesh_instance_buffer_index].Load<MeshInstance>(instance_index * sizeof(MeshInstance));
}

struct VertexInput {
  uint vertex_index : SV_VertexID;
  uint instance_index : SV_InstanceID;
  [[vk::builtin("DrawIndex")]] uint draw_index : DrawIndex;

  MeshInstancePointer GetInstancePointer(const uint instance_offset) { return MeshInstancePointers[instance_offset + instance_index]; }

  MeshInstance GetInstance(const uint instance_offset) { return load_instance(GetInstancePointer(instance_offset).GetInstanceIndex()); }

  float3 GetVertexPosition(uint64_t ptr) {
    uint64_t addressOffset = ptr + vertex_index * sizeof(Vertex);
    return vk::RawBufferLoad<float4>(addressOffset).xyz;
  }

  float3 GetVertexNormal(uint64_t ptr) {
    uint64_t addressOffset = ptr + vertex_index * sizeof(Vertex) + sizeof(float4);
    return vk::RawBufferLoad<float4>(addressOffset).xyz;
  }

  float2 GetVertexUV(uint64_t ptr) {
    uint64_t addressOffset = ptr + vertex_index * sizeof(Vertex) + sizeof(float4) * 2;
    return vk::RawBufferLoad<float4>(addressOffset).xy;
  }

  float4 GetVertexTangent(uint64_t ptr) {
    uint64_t addressOffset = ptr + vertex_index * sizeof(Vertex) + sizeof(float4) * 3;
    return vk::RawBufferLoad<float4>(addressOffset);
  }
};

// scene textures
Texture2D<float4> GetPBRTexture() { return SceneTextures[GetScene().indices_.pbr_image_index]; }
Texture2D<float4> GetNormalTexture() { return SceneTextures[GetScene().indices_.normal_image_index]; }
Texture2D<float4> GetDepthTexture() { return SceneTextures[GetScene().indices_.depth_image_index]; }
Texture2D<float4> GetShadowAtlas() { return SceneTextures[GetScene().indices_.shadow_array_index]; }
Texture2D<float4> GetSkyTransmittanceLUTTexture() { return SceneTextures[GetScene().indices_.sky_transmittance_lut_index]; }
Texture2D<float4> GetSkyMultiScatterLUTTexture() { return SceneTextures[GetScene().indices_.sky_multiscatter_lut_index]; }

// scene r/w textures
RWTexture2D<float4> GetSkyTransmittanceLUTRWTexture() { return SceneRWTextures[GetScene().indices_.sky_transmittance_lut_index]; }
RWTexture2D<float4> GetSkyMultiScatterLUTRWTexture() { return SceneRWTextures[GetScene().indices_.sky_multiscatter_lut_index]; }

// scene cube textures
TextureCube<float4> GetSkyEnvMapTexture() { return SceneCubeTextures[GetScene().indices_.sky_env_map_index]; }

// scene uint textures
Texture2D<uint> GetGTAOTexture() { return SceneUintTextures[GetScene().indices_.gtao_buffer_image_index]; }

// material textures
SamplerState GetMaterialSampler(Material material) {
  uint index = material.sampler > 1 ? 2 : 1;
  return Samplers[index];
}

Texture2D<float4> GetMaterialAlbedoTexture(Material material) { return MaterialTextureMaps[material.albedo_map_id]; }
Texture2D<float4> GetMaterialNormalTexture(Material material) { return MaterialTextureMaps[material.normal_map_id]; }
Texture2D<float4> GetMaterialPhysicalTexture(Material material) { return MaterialTextureMaps[material.physical_map_id]; }
Texture2D<float4> GetMaterialAOTexture(Material material) { return MaterialTextureMaps[material.ao_map_id]; }
Texture2D<float4> GetMaterialEmissiveTexture(Material material) { return MaterialTextureMaps[material.emissive_map_id]; }

ShaderEntity GetEntity(uint index) { return Buffers[GetScene().indices_.entites_buffer_index].Load<ShaderEntity>(index * sizeof(ShaderEntity)); }

Material GetMaterial(int material_index) {
  return Buffers[GetScene().indices_.materials_buffer_index].Load<Material>(material_index * sizeof(Material));
}
Light GetLight(int lightIndex) { return Buffers[GetScene().indices_.lights_buffer_index].Load<Light>(lightIndex * sizeof(Light)); }
#endif // GLOBALS_HLSLI
