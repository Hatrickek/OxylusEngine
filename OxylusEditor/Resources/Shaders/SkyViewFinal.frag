#version 460 core
#pragma shader_stage(fragment)

#define PI 3.14159265358

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 result;

layout(binding = 0) uniform sampler2D sky_view_lut;

layout(set = 0, binding = 1) uniform SunInfo {
    vec3 sun_dir;
    float sun_radius;
    vec4 FrustumX;
    vec4 FrustumY;
    vec4 FrustumZ;
    vec4 FrustumW;
};

vec3 GetSun(vec3 rayDir, vec3 sunDir) 
{
    float sunCos = dot(rayDir, sunDir);
    float radCos = cos(sun_radius * PI / 180.0);
    if (sunCos > radCos)
    {
        return vec3(10.0);
    }

    return vec3(0.0);
}

void main()
{
    vec4 direction = normalize(
        mix(
            mix(FrustumX, FrustumY, inUV.x),
            mix(FrustumZ, FrustumW, inUV.x),
            inUV.y)
    );

    float l = asin(direction.y);
    float u = atan(direction.z, direction.x) / (2.0 * PI);
    float v = 0.5 - 0.5 * sign(l) * sqrt(abs(l) / (0.5 * PI));

    vec3 color = texture(sky_view_lut, vec2(u, v)).rgb;
    color += GetSun(direction.xyz, sun_dir);
    result = vec4(color, 1.0);
}