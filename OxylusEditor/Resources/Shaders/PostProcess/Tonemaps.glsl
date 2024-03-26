vec3 Uncharted2TonemapOp(vec3 x) {
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;

    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 TonemapUncharted2(vec3 color) {
    float W = 11.2;
    return Uncharted2TonemapOp(2.0 * color) / Uncharted2TonemapOp(vec3(W));
}

vec3 TonemapFilmic(const vec3 color) {
    vec3 x = max(vec3(0.0), color - 0.004);
    return (x * (6.2 * x + 0.5)) / (x * (6.2 * x + 1.7) + 0.06);
}

vec3 TonemapAces(const vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), vec3(0.0), vec3(1.0));
}

vec3 TonemapReinhard(vec3 color) { return color / (1 + color); }

vec3 ApplyTonemap(vec3 color, int index) {
    vec3 finalColor;
    switch (index) {
        case 0:
          finalColor = TonemapAces(color);
          break;
        case 1:
          finalColor = TonemapUncharted2(color);
          break;
        case 2:
          finalColor = TonemapFilmic(color);
          break;
        case 3:
          finalColor = TonemapReinhard(color);
          break;
        default:
          finalColor = TonemapAces(color);
          break;
    }
    return finalColor;
}