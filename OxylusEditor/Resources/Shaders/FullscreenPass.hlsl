struct VSOutput {
  float4 Position : SV_Position;
  float2 UV : TEXCOORD;
};

VSOutput main(uint vertexID : SV_VertexID) {
  VSOutput output;
  output.UV = float2((vertexID << 1) & 2, vertexID & 2);
  output.Position = float4(output.UV * 2.0f - 1.0f, 0.0f, 1.0f);
  return output;
}
