vec3 SampleLUT(sampler2D transmittance_lut, float altitude, float theta, float atmos_radius, float planet_radius) {
    vec2 uv = vec2(0.5 + 0.5 * theta, max(0.0, min(altitude / (atmos_radius - planet_radius), 1.0)));
    return texture(transmittance_lut, uv).xyz;
}