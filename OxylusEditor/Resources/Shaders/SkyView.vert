#version 450
#pragma shader_stage(vertex)

layout(location = 0) out vec2 outUV;

out gl_PerVertex { vec4 gl_Position; };

void main() {
  outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
  gl_Position = vec4(outUV * vec2(2, -2) + vec2(-1, 1), 0.5, 1);
}