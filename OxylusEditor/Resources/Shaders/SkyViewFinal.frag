#version 460 core
#pragma shader_stage(fragment)

#define PI 3.14159265358

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 result;

layout(binding = 0) uniform sampler2D sky_view_lut;

layout(set = 0, binding = 1) uniform SunInfo {
    vec3 sun_dir;
    float sun_radius;
    vec3 FrustumX;
    vec3 FrustumY;
    vec3 FrustumZ;
    vec3 FrustumW;
};

vec3 TonemapAces(const vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), vec3(0.0), vec3(1.0));
}

vec3 dither(vec2 seed, vec3 color)
{
    color *= 255.0;
    float rand = fract(sin(dot(seed, vec2(12.9898, 78.233) * 2.0)) * 43758.5453);
    color = all(lessThan(vec3(rand), color - floor(color))) ? ceil(color) : floor(color);
    color /= 255.0;

    return color;
}

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
    vec3 direction = normalize(
        mix(
            mix(FrustumX, FrustumY, inUV.x),
            mix(FrustumZ, FrustumW, inUV.x),
            inUV.y)
    );

    float l = asin(direction.y);
    float u = atan(direction.z, direction.x) / (2.0 * PI);
    float v = 0.5 - 0.5 * sign(l) * sqrt(abs(l) / (0.5 * PI));

    vec3 color = texture(sky_view_lut, vec2(u, v)).rgb;
    color = TonemapAces(color);
    color = dither(inUV, color);
    color += GetSun(direction, sun_dir);
    result = vec4(color, 1.0);
}