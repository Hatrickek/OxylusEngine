#version 450
#pragma shader_stage(vertex)

layout(location = 0) out vec2 out_UV;

out gl_PerVertex { vec4 gl_Position; };

void main() {
  out_UV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
  gl_Position = vec4(out_UV * 2.0f - 1.0f, 0.0f, 1.0f);
}