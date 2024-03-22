#ifndef OBJECTVS_HLSL
#define OBJECTVS_HLSL

#ifdef DEPTH_PASS
  #define USE_POSITION
  #define USE_WORLD_POS
  #define USE_NORMAL
  #define USE_UV
  #define OUTPUT_NORMAL_VS
#endif

#ifdef SHADOW_PASS
  #define USE_POSITION
  #define USE_VIEWPORT
#endif

#ifdef FORWARD_PASS
  #define USE_POSITION
  #define USE_WORLD_POS
  #define USE_NORMAL
  #define USE_UV
  #define USE_VIEWPOS
  #define USE_TANGENT
#endif

#include "Globals.hlsli"

[[vk::push_constant]] PushConst push_const;

VertexOutput VSmain(VertexInput input) {
  const float3 vertex_position = input.GetVertexPosition(push_const.vertex_buffer_ptr);
  const float3 vertex_normal = input.GetVertexNormal(push_const.vertex_buffer_ptr);
  const float2 vertex_uv = input.GetVertexUV(push_const.vertex_buffer_ptr);
  const float4 tangent = input.GetVertexTangent(push_const.vertex_buffer_ptr);

  const MeshInstance mesh_instance = input.GetInstance(push_const.instance_offset);

  const float4x4 model_matrix = mesh_instance.transform;

  const float4 loc_pos = mul(model_matrix, float4(vertex_position.xyz, 1.0));

  VertexOutput output;

#ifdef USE_NORMAL
  output.normal = normalize(mul(model_matrix, vertex_normal)).xyz;
  #ifdef OUTPUT_NORMAL_VS
  output.normal =
    mul(GetCamera(input.GetInstancePointer(push_const.instance_offset).GetCameraIndex()).view_matrix, output.normal); // normals in viewspace
  #endif
#endif

#ifdef USE_UV
  output.uv = vertex_uv.xy;
#endif
#ifdef USE_WORLD_POS
  output.world_pos = loc_pos.xyz;
#endif
#ifdef USE_POSITION
  output.position =
    mul(GetCamera(input.GetInstancePointer(push_const.instance_offset).GetCameraIndex()).projection_view_matrix, float4(loc_pos.xyz, 1.0));
#endif
#ifdef USE_VIEWPOS
  output.view_pos = mul(GetCamera(input.GetInstancePointer(push_const.instance_offset).GetCameraIndex()).view_matrix, float4(loc_pos.xyz, 1.0f)).xyz;
#endif
#ifdef USE_TANGENT
  output.tangent = mul(model_matrix, tangent);
#endif
#ifdef USE_VIEWPORT
  output.vp_index = GetCamera(input.GetInstancePointer(push_const.instance_offset).GetCameraIndex()).output_index;
#endif

  output.draw_index = input.draw_index;

  return output;
}

#endif
