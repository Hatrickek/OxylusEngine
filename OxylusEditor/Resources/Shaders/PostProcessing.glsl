vec3 ApplyVignette(vec4 vignetteOffset, vec4 vignetteColor, vec2 texCoords) {
  vec3 finalColor = vec3(1);
  if (vignetteOffset.z > 0.0) {
    float vignetteOpacity =
        /* (1.0 - texture(vignetteMask, texCoords).r) *  */ vignetteColor.a;
    finalColor = mix(finalColor, vignetteColor.rgb, vignetteOpacity);
  } else {
    vec2 uv = texCoords + vignetteOffset.xy;
    uv *= 1.0 - (texCoords.yx + vignetteOffset.yx);
    float vig = uv.x * uv.y * 15.0;
    vig = pow(vig, vignetteColor.a);
    vig = clamp(vig, 0.0, 1.0);

    finalColor = mix(vignetteColor.rgb, finalColor, vig);
  }
  return finalColor;
}
