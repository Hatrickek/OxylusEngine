#include "SkyCommonShading.hlsli"

struct VertexOutput {
  float4 pos : SV_POSITION;
  float3 nor : NORMAL;
  uint RTIndex : SV_RenderTargetArrayIndex;
};

VertexOutput VSmain(Vertex inVertex, uint vid : SV_VERTEXID, uint instanceID : SV_INSTANCEID) {
  VertexOutput output = (VertexOutput)0;
  output.RTIndex = instanceID;
  output.pos = mul(GetScene().CubemapViewProjections[output.RTIndex], float4(inVertex.Position, 1.0));
  output.nor = inVertex.Normal;
  return output;
}

float4 PSmain(VertexOutput input) : SV_TARGET {
  const float3 normal = normalize(input.nor);

  // No direct sun should be visible inside the probe capture:
  float4 color = float4(GetDynamicSkyColor(input.pos.xy, normal, false, false, false, false, false, false), 1);

  color = clamp(color, 0, 65000);
  return float4(color.rgb, 1);
}
