vec3 PP_Vignette(vec4 vignetteOffset, vec4 vignetteColor, vec2 texCoords) {
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

float hash(vec2 vec) {
  // "Hash without Sine", from https://www.shadertoy.com/view/4djSRW
  vec3 v3 = fract(vec3(vec.xyx) * 0.1031);
  v3 += dot(v3, v3.yzx + 33.33);
  return fract((v3.x + v3.y) * v3.z);
}

float PP_FilmGrain(vec2 uv, float strength) { return (hash(uv * vec2(312.24, 1030.057) /*  * uniGlobalTime */) * 2.0 - 1.0) * strength; }

vec3 PP_ChromaticAberration(sampler2D colorTexture, vec2 uv, float amount) {
  float redOffset = 0.009 * amount;
  float greenOffset = 0.006 * amount;
  float blueOffset = -0.006 * amount;

  vec2 mouseFocusPoint = vec2(0, 0);
  vec2 direction = uv - mouseFocusPoint;

  vec3 finalColor = texture(colorTexture, uv).rgb;

  finalColor.r = texture(colorTexture, uv + (direction * vec2(redOffset))).r;
  finalColor.g = texture(colorTexture, uv + (direction * vec2(greenOffset))).g;
  finalColor.b = texture(colorTexture, uv + (direction * vec2(blueOffset))).b;

  return finalColor;
}

vec3 PP_Sharpen(sampler2D sharpenTexture, float amount) {
  float neighbor = amount * -1;
  float center = amount * 4 + 1;

  vec2 texSize = textureSize(sharpenTexture, 0);

  vec3 color = texture(sharpenTexture, vec2(gl_FragCoord.x + 0, gl_FragCoord.y + 1) / texSize).rgb * neighbor +
               texture(sharpenTexture, vec2(gl_FragCoord.x - 1, gl_FragCoord.y + 0) / texSize).rgb * neighbor +
               texture(sharpenTexture, vec2(gl_FragCoord.x + 0, gl_FragCoord.y + 0) / texSize).rgb * center +
               texture(sharpenTexture, vec2(gl_FragCoord.x + 1, gl_FragCoord.y + 0) / texSize).rgb * neighbor +
               texture(sharpenTexture, vec2(gl_FragCoord.x + 0, gl_FragCoord.y - 1) / texSize).rgb * neighbor;

  return color;
}
