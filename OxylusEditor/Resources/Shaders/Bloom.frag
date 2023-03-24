#version 450

const float EPSILON = 1.0e-4;

layout(binding = 0) uniform UBO {
  vec4 Threshold; // x: threshold value (linear), y: knee, z: knee * 2, w: 0.25
                  // / knee
  vec4 Params;    // x: clamp, y: <1-unsampled, <2-downsample, <3-upsample, z:
                  // <1-prefilter, <=1-no prefilter, w: unused
}
u_Ubo;

layout(binding = 1) uniform sampler2D u_Texture;
layout(binding = 2) uniform sampler2D u_AdditiveTexture;

layout(location = 0) in vec2 in_TexCoord;

layout(location = 0) out vec4 out_FragColor;

// Better, temporally stable box filtering
// [Jimenez14] http://goo.gl/eomGso
vec4 DownsampleBox13Tap(sampler2D tex, vec2 uv, vec2 texelSize) {
  vec4 A = texture(tex, uv + texelSize * vec2(-1.0, -1.0));
  vec4 B = texture(tex, uv + texelSize * vec2(0.0, -1.0));
  vec4 C = texture(tex, uv + texelSize * vec2(1.0, -1.0));
  vec4 D = texture(tex, uv + texelSize * vec2(-0.5, -0.5));
  vec4 E = texture(tex, uv + texelSize * vec2(0.5, -0.5));
  vec4 F = texture(tex, uv + texelSize * vec2(-1.0, 0.0));
  vec4 G = texture(tex, uv);
  vec4 H = texture(tex, uv + texelSize * vec2(1.0, 0.0));
  vec4 I = texture(tex, uv + texelSize * vec2(-0.5, 0.5));
  vec4 J = texture(tex, uv + texelSize * vec2(0.5, 0.5));
  vec4 K = texture(tex, uv + texelSize * vec2(-1.0, 1.0));
  vec4 L = texture(tex, uv + texelSize * vec2(0.0, 1.0));
  vec4 M = texture(tex, uv + texelSize * vec2(1.0, 1.0));

  vec2 div = (1.0 / 4.0) * vec2(0.5, 0.125);

  vec4 o = (D + E + I + J) * div.x;
  o += (A + B + G + F) * div.y;
  o += (B + C + H + G) * div.y;
  o += (F + G + L + K) * div.y;
  o += (G + H + M + L) * div.y;

  return o;
}

// Standard box filtering
vec4 DownsampleBox4Tap(sampler2D tex, vec2 uv, vec2 texelSize) {
  vec4 d = texelSize.xyxy * vec4(-1.0, -1.0, 1.0, 1.0);

  vec4 s = texture(tex, uv + d.xy);
  s += texture(tex, uv + d.zy);
  s += texture(tex, uv + d.xw);
  s += texture(tex, uv + d.zw);

  return s * (1.0 / 4.0);
}

// 9-tap bilinear upsampler (tent filter)
vec4 UpsampleTent(sampler2D tex, vec2 uv, vec2 texelSize, vec4 sampleScale) {
  vec4 d = texelSize.xyxy * vec4(1.0, 1.0, -1.0, 0.0) * sampleScale;

  vec4 s = texture(tex, uv - d.xy);
  s += texture(tex, uv - d.wy) * 2.0;
  s += texture(tex, uv - d.zy);

  s += texture(tex, uv + d.zw) * 2.0;
  s += texture(tex, uv) * 4.0;
  s += texture(tex, uv + d.xw) * 2.0;

  s += texture(tex, uv + d.zy);
  s += texture(tex, uv + d.wy) * 2.0;
  s += texture(tex, uv + d.xy);

  return s * (1.0 / 16.0);
}

// Standard box filtering
vec4 UpsampleBox(sampler2D tex, vec2 uv, vec2 texelSize, vec4 sampleScale) {
  vec4 d = texelSize.xyxy * vec4(-1.0, -1.0, 1.0, 1.0) * (sampleScale * 0.5);

  vec4 s = texture(tex, uv + d.xy);
  s += texture(tex, uv + d.zy);
  s += texture(tex, uv + d.xw);
  s += texture(tex, uv + d.zw);

  return s * (1.0 / 4.0);
}

void main() {
  vec2 texelSize = 1.0 / textureSize(u_Texture, 0);
  vec4 color = vec4(0.0);
  if (u_Ubo.Params.y <= 1.01) {
    color = texture(u_Texture, in_TexCoord);
  } else if (u_Ubo.Params.y <= 2.01) {
    color = DownsampleBox13Tap(u_Texture, in_TexCoord, texelSize);
  } else if (u_Ubo.Params.y <= 3.01) {
    color = UpsampleTent(u_Texture, in_TexCoord, texelSize, vec4(1.0));
  }

  if (u_Ubo.Params.z < 0.5) {
    // User controlled clamp to limit crazy high broken spec
    color = min(vec4(u_Ubo.Params.x), color);

    float brightness = max(color.r, color.g);
    brightness = max(brightness, color.b);
    float softness = clamp(brightness - u_Ubo.Threshold.x + u_Ubo.Threshold.y,
                           0.0, u_Ubo.Threshold.z);
    softness = (softness * softness) / (4.0 * u_Ubo.Threshold.y + EPSILON);
    float multiplier = max(brightness - u_Ubo.Threshold.x, softness) /
                       max(brightness, EPSILON);
    color *= multiplier;
    color = max(color, vec4(0.0, 0.0, 0.0, 1.0));
  }

  if (u_Ubo.Params.w > 0.0) {
    color += texture(u_AdditiveTexture, in_TexCoord);
  }

  out_FragColor = vec4(color.rgb, 1.0);
}
