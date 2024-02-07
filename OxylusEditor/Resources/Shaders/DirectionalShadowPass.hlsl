#include "Globals.hlsli"

[[vk::push_constant]]
struct PushConstant {
  float4x4 ModelMatrix;
  uint64_t VertexBufferPtr;
  uint32_t CascadeIndex;
  float _pad;
} PushConst;

struct VSLayout {
  float4 Position : SV_POSITION;
};

VSLayout VSmain(uint vertexIndex : SV_VertexID) {
  const float4 vertexPosition = vk::RawBufferLoad<float4>(PushConst.VertexBufferPtr + vertexIndex * sizeof(Vertex));

  VSLayout output;
  output.Position = mul(mul(GetScene().CascadeViewProjections[PushConst.CascadeIndex], PushConst.ModelMatrix), float4(vertexPosition.xyz, 1.0));
  
  return output;
}

