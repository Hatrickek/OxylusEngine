#version 450
const int BLUR_RADIUS = 2;

layout (binding = 0) uniform sampler2D in_SSAO;
layout (binding = 1) uniform sampler2D in_Depth;
layout (binding = 2) uniform sampler2D in_Normal;

layout (push_constant) uniform PushConst{
    vec4 ssaoTexelOffset; //xy offset, z near clip, w far clip 
}u_PushConst;

layout (location = 1) in vec2 in_TexCoord;

layout (location = 0) out float out_Color;

void main()
{
	float ourDepth = texture(in_Depth, in_TexCoord).r;
	vec3 ourNormal = normalize(texture(in_Normal, in_TexCoord).rgb * 2.0f - 1.0f);
	
	int sampleCount = 0;
	float sum = 0.0f;
	for (int i = -BLUR_RADIUS; i <= BLUR_RADIUS; i++) 
	{
		vec2 offset = u_PushConst.ssaoTexelOffset.xy * float(i);
		float depth = texture(in_Depth, in_TexCoord + offset).r;
		vec3 normal = normalize(texture(in_Normal, in_TexCoord + offset).rgb * 2.0f - 1.0f);
		//if (abs(ourDepth - depth) < 0.00002f && dot(ourNormal, normal) > 0.85f)
		//{
			sum += texture(in_SSAO, in_TexCoord + offset).r;
			++sampleCount;
		//}
	}
	out_Color = clamp(sum / float(sampleCount), 0.0f, 1.0f);
}