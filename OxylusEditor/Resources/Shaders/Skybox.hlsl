#include "Globals.hlsli"

struct VSLayout {
  float4 Position : POSITION;
  float3 UVW : TEXCOORD;
};

[[vk::push_constant]]
struct PushConstant {
  float4x4 ViewMatrix;
} PushConst;

VSLayout VSmain(Vertex inVertex) {
  VSLayout output;
  output.Position = mul(GetCamera().ProjectionMatrix * PushConst.ViewMatrix, float4(inVertex.Position, 1.0));
  output.UVW = inVertex.Position;

  return output;
}
