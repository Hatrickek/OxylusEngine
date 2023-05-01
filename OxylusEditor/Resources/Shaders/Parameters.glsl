struct Parameters {
  int Tonemapper;  // 0- Aces 1- Uncharted 2-Filmic 3- Reinhard
  float Exposure;
  float Gamma;
  bool EnableSSAO;
  bool EnableBloom;
  bool EnableSSR;
  vec2 _pad;
  vec4 VignetteColor;        // rgb: color, a: intensity
  vec4 VignetteOffset;       // xy: offset, z: useMask, w: enable/disable effect
  vec2 FilmGrain;            // x: enable, y: amount
  vec2 ChromaticAberration;  // x: enable, y: amount
  vec2 Sharpen;              // x: enable, y: amount
};