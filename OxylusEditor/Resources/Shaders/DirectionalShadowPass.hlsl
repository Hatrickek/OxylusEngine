#include "Globals.hlsli"

[[vk::push_constant]]
struct PushConstant {
  uint64_t MeshIntancesBufferPtr;
  uint64_t VertexBufferPtr;
  uint32_t MeshIndex;
  uint32_t CascadeIndex;
  float2 _pad;
} PushConst;

struct VSLayout {
  float4 Position : SV_POSITION;
};

VSLayout VSmain(uint vertexIndex : SV_VertexID, uint instanceIndex : SV_InstanceID) {
  const float4 vertexPosition = vk::RawBufferLoad<float4>(PushConst.VertexBufferPtr + vertexIndex * sizeof(Vertex));

  uint64_t addressOffset = PushConst.MeshIntancesBufferPtr + (PushConst.MeshIndex + instanceIndex) * sizeof(MeshInstance);
  const float4x4 modelMatrix = transpose(vk::RawBufferLoad<float4x4>(addressOffset));
  VSLayout output;
  output.Position = mul(mul(GetScene().CascadeViewProjections[PushConst.CascadeIndex], modelMatrix), float4(vertexPosition.xyz, 1.0));
  
  return output;
}

