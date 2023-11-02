vec4 SRGBtoLINEAR(vec4 srgbIn) {
	#ifdef MANUAL_SRGB
	#ifdef SRGB_FAST_APPROXIMATION
	vec3 linOut = pow(srgbIn.xyz,vec3(2.2));
	#else //SRGB_FAST_APPROXIMATION
	vec3 bLess = step(vec3(0.04045),srgbIn.xyz);
	vec3 linOut = mix( srgbIn.xyz/vec3(12.92), pow((srgbIn.xyz+vec3(0.055))/vec3(1.055),vec3(2.4)), bLess );
	#endif //SRGB_FAST_APPROXIMATION
	return vec4(linOut,srgbIn.w);;
	#else //MANUAL_SRGB
	return srgbIn;
	#endif //MANUAL_SRGB
}

vec3 R11G11B10_UNORM_to_FLOAT3(uint packedInput) {
  vec3 unpackedOutput;
  unpackedOutput.x = float(((packedInput) & 0x000007ff) / 2047.0f);
  unpackedOutput.y = float(((packedInput >> 11) & 0x000007ff) / 2047.0f);
  unpackedOutput.z = float(((packedInput >> 22) & 0x000003ff) / 1023.0f);
  return unpackedOutput;
}

uint FLOAT3_to_R11G11B10_UNORM(vec3 unpackedInput) {
    uint packedOutput;
    packedOutput =((uint(clamp(unpackedInput.x, 0.0, 1.0) * 2047 + 0.5) ) |
                   (uint(clamp(unpackedInput.y, 0.0, 1.0) * 2047 + 0.5) << 11 ) |
                   (uint(clamp(unpackedInput.z, 0.0, 1.0) * 1023 + 0.5) << 22 ) );
    return packedOutput;
}

vec4 R8G8B8A8_UNORM_to_FLOAT4( uint packedInput )
{
    vec4 unpackedOutput;
    unpackedOutput.x = float(( packedInput & 0x000000ff ) / 255);
    unpackedOutput.y = float(( ( ( packedInput >> 8 ) & 0x000000ff ) ) / 255);
    unpackedOutput.z = float(( ( ( packedInput >> 16 ) & 0x000000ff ) ) / 255);
    unpackedOutput.w = float(( packedInput >> 24 ) / 255);
    return unpackedOutput;
}