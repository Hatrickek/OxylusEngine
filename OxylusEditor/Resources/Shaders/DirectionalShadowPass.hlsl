#include "Globals.hlsli"

[[vk::push_constant]]
struct PushConstant {
  float4x4 ModelMatrix;
  uint64_t VertexBufferPtr;
  uint32_t CascadeIndex;
} PushConst;

struct VSLayout {
  float4 Position : POSITION;
};

VSLayout VSmain(uint vertexIndex : SV_VertexID) {
  const float3 vertexPosition = vk::RawBufferLoad<float3>(PushConst.VertexBufferPtr + vertexIndex * sizeof(Vertex));

  VSLayout output;
  output.Position = mul(GetScene().CascadeViewProjections[PushConst.CascadeIndex] * PushConst.ModelMatrix, float4(vertexPosition, 1.0));

  return output;
}

float4 PSmain(VSLayout input) : SV_Target {
  return float4(0.0, 0.0, 0.0, 0.0);
}
