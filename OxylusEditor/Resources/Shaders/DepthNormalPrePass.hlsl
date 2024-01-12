#include "Globals.hlsli"

[[vk::push_constant]]
struct PushConstant {
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
  float3x3 WorldTangent : WORLD_TANGENT;
};

VSLayout VSmain(uint vertexIndex : SV_VertexID) {
  const float3 vertexPosition = vk::RawBufferLoad<float3>(PushConst.VertexBufferPtr + vertexIndex * sizeof(Vertex));
  const float3 vertexNormal = vk::RawBufferLoad<float3>(PushConst.VertexBufferPtr + vertexIndex * sizeof(Vertex) + 16);
  const float2 vertexUV = vk::RawBufferLoad<float2>(PushConst.VertexBufferPtr + vertexIndex * sizeof(Vertex) + 32);
  const float4 vertexTangent = vk::RawBufferLoad<float4>(PushConst.VertexBufferPtr + vertexIndex * sizeof(Vertex) + 48);

  const float4 locPos = mul(PushConst.ModelMatrix, float4(vertexPosition, 1.0));

  float3 T = normalize(mul(PushConst.ModelMatrix, float4(vertexTangent.xyz, 0.0)).xyz);
  float3 N = normalize(mul(PushConst.ModelMatrix, float4(vertexNormal, 0.0)).xyz);
  T = normalize(T - dot(T, N) * N);
  float3 B = cross(N, T);

  VSLayout output;
  output.Normal = normalize(mul(transpose((float3x3)PushConst.ModelMatrix), vertexNormal));
  output.WorldPos = locPos.xyz / locPos.w;
  output.UV = vertexUV;
  output.Position = mul(Camera.ProjectionMatrix * Camera.ViewMatrix, float4(output.WorldPos, 1.0));
  output.WorldTangent = float3x3(T, B, N);

  return output;
}

float4 PSmain(VSLayout input) : SV_Target {
  Material mat = Materials[PushConst.MaterialIndex].Load<Material>(0);

  float2 scaledUV = input.UV;
  scaledUV *= mat.UVScale;

  const float normalMapStrenght = mat.UseNormal ? 1.0 : 0.0;
  float3 normal = GetMaterialNormalTexture(PushConst.MaterialIndex).Sample(LINEAR_REPEATED_SAMPLER, scaledUV).rgb;
  normal = mul(input.WorldTangent, normalize(normal * 2.0 - 1.0));
  normal = lerp(normalize(input.Normal), normal, normalMapStrenght);
  normal = normalize(mul((float3x3)Camera.ViewMatrix, normal));

  float invRoughness;
  if (mat.UsePhysicalMap) {
    invRoughness = GetMaterialPhysicalTexture(PushConst.MaterialIndex).Sample(LINEAR_REPEATED_SAMPLER, scaledUV).g;
    invRoughness *= 1.0 - mat.Roughness;
  }
  else {
    invRoughness = 1.0 - mat.Roughness;
  }

  return float4(normal, invRoughness);
}
