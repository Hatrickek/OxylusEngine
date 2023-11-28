#version 460 core
#pragma shader_stage(fragment)

#define PI 3.14159265358

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 result;

layout(binding = 0) uniform sampler2D transmittanceLUT;

layout(set = 0, binding = 1) uniform UniformBlock {
    vec3 RayleighScatterVal;
    float RayleighDensity;
    float PlanetRadius;
    float AtmosRadius;
    float MieScatterVal;
    float MieAbsorptionVal;
    float MieDensity;
    float MieAsymmetry;
    float OzoneHeight;
    float OzoneThickness;
    vec3 OzoneAbsorption;
};

layout(set = 0, binding = 2) uniform EyeView {
    vec3 EyePosition;
    float StepCount;
    vec3 SunDirection;
    float SunIntensity;
};

vec3 SampleLUT(float altitude, float theta)
{
    vec2 uv = vec2(0.5 + 0.5 * theta, max(0.0, min(altitude / (AtmosRadius - PlanetRadius), 1.0)));
    return texture(transmittanceLUT, uv).xyz;
}

vec3 GetExtinctionSum(float altitude)
{
    vec3 rayleigh = RayleighScatterVal * exp(-altitude / RayleighDensity);
    float mie = (MieScatterVal + MieAbsorptionVal) * exp(-altitude / MieDensity);
    vec3 ozone = OzoneAbsorption * max(0.0, 1.0 - abs(altitude - OzoneHeight) / OzoneThickness);

    return rayleigh + mie + ozone;
}

void GetScattering(float altitude, out vec3 rayleigh, out float mie)
{
    rayleigh = RayleighScatterVal * exp(-altitude / RayleighDensity);
    mie = (MieScatterVal + MieAbsorptionVal) * exp(-altitude / MieDensity);
}

float GetRayleighPhase(float altitude)
{
    const float k = 3.0 / (16.0 * PI);
    return k * (1.0 + altitude * altitude);
}

float GetMiePhase(float altitude)
{
    const float g = MieAsymmetry;
    const float g2 = g * g;
    const float scale = 3.0 / (8.0 * PI);

    float num = (1.0 - g2) * (1.0 + altitude * altitude);
    float denom = (2.0 + g2) * pow(abs(1.0 + g2 - 2.0 * g * altitude), 1.5);
    
    return scale * num / denom;
}

bool SolveQuadratic(vec3 origin, vec3 direction, float radius)
{
    float a = dot(direction, direction);
    float b = 2.0 * dot(origin, direction);
    float c = dot(origin, origin) - radius * radius;
    float discriminant = b * b - 4.0 * a * c;

    return (discriminant >= 0.0) && (b <= 0.0);
}

// Out T = distance from origin
bool GetQuadraticIntersection3D(vec3 origin, vec3 direction, float radius, out float t)
{
    float a = dot(direction, direction);
    float b = 2.0 * dot(origin, direction);
    float c = dot(origin, origin) - radius * radius;
    float discriminant = b * b - 4.0 * a * c;

    if (discriminant < 0.0)
    return false;

    if (c <= 0.0)
    t = (-b + sqrt(discriminant)) / (a * 2.0);
    else
    t = (-b + -sqrt(discriminant)) / (a * 2.0);

    return (b <= 0.0);
}

void main()
{
    float u = inUV.x;
    float v = inUV.y;

    // Non-linear parameterization
    if (v < 0.5) 
    {
        float coord = 1.0 - 2.0 * v;
        v = coord * coord;
    } 
    else 
    {
        float coord = v * 2.0 - 1.0;
        v = -coord * coord;
    }

    float h = length(EyePosition);

    float azimuthAngle = 2.0 * PI * u;  // Consider 360 degrees.
    float horizonAngle = acos(sqrt(h * h - PlanetRadius * PlanetRadius) / h) - 0.5 * PI;
    float altitudeAngle = v * 0.5 * PI - horizonAngle;

    float cosAltitude = cos(altitudeAngle);
    vec3 rayDirection = vec3(
        cosAltitude * cos(azimuthAngle),
        sin(altitudeAngle),
        cosAltitude * sin(azimuthAngle)
    );

     // Get Rayleigh + Mie phase
    float cosTheta = dot(rayDirection, SunDirection);
    float rayleighPhase = GetRayleighPhase(-cosTheta);
    float miePhase = GetMiePhase(cosTheta);

    vec3 luminance = vec3(0.0, 0.0, 0.0);
    vec3 transmittance = vec3(1.0, 1.0, 1.0);
    
    // Raymarching
    float maxDist = 0.0;
    if (!GetQuadraticIntersection3D(EyePosition, rayDirection, PlanetRadius, maxDist))
        GetQuadraticIntersection3D(EyePosition, rayDirection, AtmosRadius, maxDist);

    float stepSize = maxDist / StepCount;
    float t = 0.0;
    for (float i = 0.0; i < StepCount; i += 1.0)
    {
        float nextT = stepSize * i;
        float deltaT = nextT - t;
        t = nextT;
        
        vec3 stepPosition = EyePosition + t * rayDirection;
        
        h = length(stepPosition);
        
        // Altitude from ground to top atmosphere
        float altitude = h - PlanetRadius;
        vec3 extinction = GetExtinctionSum(altitude);
        vec3 altitudeTrans = exp(-deltaT * extinction);
        
        // Shadowing factor
        float sunTheta = dot(SunDirection, stepPosition / h);
        vec3 sunTrans = SampleLUT(altitude, sunTheta);
        
        // Get scattering coefficient
        vec3 rayleighScat;
        float mieScat;
        GetScattering(altitude, rayleighScat, mieScat);
        
        // Molecules scattered on ray's position
        vec3 rayleighInScat = rayleighScat * rayleighPhase;
        vec3 mieInScat = vec3(mieScat * miePhase);
        vec3 scatteringPhase = (rayleighInScat + mieInScat) * sunTrans;

        // https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite
        // slide 28
        vec3 integral = (scatteringPhase - scatteringPhase * altitudeTrans) / extinction;

        luminance += SunIntensity * (integral * transmittance);
        transmittance *= altitudeTrans;
    }

    result = vec4(luminance, 1.0);
}