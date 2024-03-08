#include "Globals.hlsli"
#include "PBRCommon.hlsli"

[[vk::push_constant]]
struct PushConstant {
  uint64_t MeshIntancesBufferPtr;
  uint64_t VertexBufferPtr;
  uint32_t MeshIndex;
  uint32_t MaterialIndex;
  float2 _pad;
} PushConst;

struct VSLayout {
  float4 Position : SV_POSITION;
  float3 WorldPos : WORLD_POSITION;
  float3 Normal : NORMAL;
  float2 UV : TEXCOORD;
};

VSLayout VSmain(uint vertexIndex : SV_VertexID, uint instanceIndex : SV_InstanceID) {
  uint64_t addressOffset = PushConst.VertexBufferPtr + vertexIndex * sizeof(Vertex);
  const float4 vertexPosition = vk::RawBufferLoad<float4>(addressOffset);
  const float4 vertexNormal = vk::RawBufferLoad<float4>(addressOffset + sizeof(float4));
  const float4 vertexUV = vk::RawBufferLoad<float4>(addressOffset + sizeof(float4) * 2);
  const float4 vertexTangent = vk::RawBufferLoad<float4>(addressOffset + sizeof(float4) * 3);

  addressOffset = PushConst.MeshIntancesBufferPtr + (PushConst.MeshIndex + instanceIndex) * sizeof(MeshInstance);
  const float4x4 modelMatrix = transpose(vk::RawBufferLoad<float4x4>(addressOffset));

  const float4 locPos = mul(modelMatrix, float4(vertexPosition.xyz, 1.0));

  float3 T = normalize(mul(modelMatrix, float4(vertexTangent.xyz, 0.0)).xyz);
  float3 N = normalize(mul(modelMatrix, float4(vertexNormal.xyz, 0.0)).xyz);
  T = normalize(T - dot(T, N) * N);
  float3 B = cross(N, T);

  VSLayout output;
  output.Normal = normalize(mul((float3x3)modelMatrix, vertexNormal));
  output.Normal = mul(GetCamera().ViewMatrix, output.Normal); // normals in viewspace
  output.WorldPos = locPos.xyz / locPos.w;
  output.UV = vertexUV.xy;
  output.Position = mul(mul(GetCamera().ProjectionMatrix, GetCamera().ViewMatrix), float4(output.WorldPos, 1.0));

  return output;
}

float4 PSmain(VSLayout input) : SV_Target0 {
  Material material = GetMaterial(PushConst.MaterialIndex);

  float2 scaledUV = input.UV;
  scaledUV *= material.UVScale;

  float4 baseColor;
  const SamplerState materialSampler = Samplers[material.Sampler];
  if (material.AlbedoMapID != INVALID_ID) {
    baseColor = GetMaterialAlbedoTexture(material).Sample(materialSampler, scaledUV) * material.Color;
  }
  else {
    baseColor = material.Color;
  }

  if (material.AlphaMode == ALPHA_MODE_MASK) {
    clip(baseColor.a - material.AlphaCutoff);
  }

  float3 normal = normalize(input.Normal);

  const bool useNormalMap = material.NormalMapID != INVALID_ID;
  if (useNormalMap) {
    const SamplerState materialSampler = Samplers[material.Sampler];
    float3 bumpColor = GetMaterialNormalTexture(material).Sample(materialSampler, scaledUV).rgb;
    bumpColor = bumpColor * 2.f - 1.f;

    const float3x3 TBN = GetNormalTangent(input.WorldPos, input.Normal, scaledUV);
    normal = normalize(lerp(normal, mul(bumpColor, TBN), length(bumpColor)));
  }

  return float4(normal, 1.0f);
}
