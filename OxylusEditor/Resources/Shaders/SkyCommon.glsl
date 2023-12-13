vec3 SampleLUT(sampler2D transmittanceLUT, float altitude, float theta, float atmosRadius, float planetRadius) {
    vec2 uv = vec2(0.5 + 0.5 * theta, max(0.0, min(altitude / (atmosRadius - planetRadius), 1.0)));
    return texture(transmittanceLUT, uv).xyz;
}