#include "Globals.hlsli"

[[vk::push_constant]]
struct PushConstant {
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
  float3x3 WorldTangent : WORLD_TANGENT;
};

VSLayout VSmain(uint vertexIndex : SV_VertexID) {
  uint64_t addressOffset = PushConst.VertexBufferPtr + vertexIndex * sizeof(Vertex);
  const float4 vertexPosition = vk::RawBufferLoad<float4>(addressOffset);
  const float4 vertexNormal = vk::RawBufferLoad<float4>(addressOffset + sizeof(float4));
  const float4 vertexUV = vk::RawBufferLoad<float4>(addressOffset + sizeof(float4) * 2);
  const float4 vertexTangent = vk::RawBufferLoad<float4>(addressOffset + sizeof(float4) * 3);

  const float4 locPos = mul(PushConst.ModelMatrix, float4(vertexPosition.xyz, 1.0));

  float3 T = normalize(mul(PushConst.ModelMatrix, float4(vertexTangent.xyz, 0.0)).xyz);
  float3 N = normalize(mul(PushConst.ModelMatrix, float4(vertexNormal.xyz, 0.0)).xyz);
  T = normalize(T - dot(T, N) * N);
  float3 B = cross(N, T);

  VSLayout output;
  output.Normal = normalize(mul(transpose((float3x3)PushConst.ModelMatrix), vertexNormal));
  output.WorldPos = locPos.xyz / locPos.w;
  output.UV = vertexUV.xy;
  output.Position = mul(mul(GetCamera().ProjectionMatrix, GetCamera().ViewMatrix), float4(output.WorldPos, 1.0));
  output.WorldTangent = float3x3(T, B, N);

  return output;
}

float4 PSmain(VSLayout input) : SV_Target0 {
  Material mat = GetMaterial(PushConst.MaterialIndex);

  float2 scaledUV = input.UV;
  scaledUV *= mat.UVScale;

  bool useNormalMap = mat.NormalMapID != INVALID_ID;
  const float normalMapStrenght = useNormalMap ? 1.0 : 0.0;
  float3 normal = float3(0, 0, 0);
  if (useNormalMap)
    normal = GetMaterialNormalTexture(mat).Sample(LINEAR_REPEATED_SAMPLER, scaledUV).rgb;
  normal = mul(input.WorldTangent, normalize(normal * 2.0 - 1.0));
  normal = lerp(normalize(input.Normal), normal, normalMapStrenght);
  normal = normalize(mul((float3x3)GetCamera().ViewMatrix, normal));

  float invRoughness;
  if (mat.PhysicalMapID != INVALID_ID) {
    invRoughness = GetMaterialPhysicalTexture(mat).Sample(LINEAR_REPEATED_SAMPLER, scaledUV).g;
    invRoughness *= 1.0 - mat.Roughness;
  }
  else {
    invRoughness = 1.0 - mat.Roughness;
  }

  return float4(normal, invRoughness);
}
