#version 450
#pragma shader_stage(fragment)

layout(location = 0) in vec3 in_WorldPos;
layout(location = 1) in flat uint in_EntityID;

layout(location = 0) out uint out_ID;

void main() {
    out_ID = in_EntityID;
}