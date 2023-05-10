#define SHADOW_MAP_CASCADE_COUNT 4

float textureProj(sampler2DArray shadowMap, vec4 shadowCoord, vec2 offset, uint cascadeIndex, float ambient) {
  float shadow = 1.0;
  float bias = 0.005;

  if (shadowCoord.z > -1.0 && shadowCoord.z < 1.0) {
    float dist = texture(shadowMap, vec3(shadowCoord.st + offset, cascadeIndex)).r;
    if (shadowCoord.w > 0 && dist < shadowCoord.z - bias) {
      shadow = ambient;
    }
  }
  return shadow;
}

float FilterPCF(sampler2DArray shadowMap, vec4 shadowCoord, uint cascadeIndex, float ambient) {
  ivec2 texDim = textureSize(shadowMap, 0).xy;
  float scale = 0.75; // TODO: Make configurable
  float dx = scale * 1.0 / float(texDim.x);
  float dy = scale * 1.0 / float(texDim.y);

  float shadowFactor = 0.0;
  int count = 0;
  int range = 1; // TODO: Make configurable

  for (int x = -range; x <= range; x++) {
    for (int y = -range; y <= range; y++) {
      shadowFactor += textureProj(shadowMap, shadowCoord, vec2(dx * x, dy * y), cascadeIndex, ambient);
      count++;
    }
  }
  return shadowFactor / count;
}

uint GetCascadeIndex(vec4 cascadeSplits, vec3 viewPosition, int cascadeCount) {
  uint cascadeIndex = 0;
  for (uint i = 0; i < cascadeCount - 1; ++i) {
    if (viewPosition.z < cascadeSplits[i]) {
      cascadeIndex = i + 1;
    }
  }
  return cascadeIndex;
}

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 
);

vec4 GetShadowCoord(mat4[SHADOW_MAP_CASCADE_COUNT] cascadeViewProjMat, vec3 inPos, uint cascadeIndex) {
  return (biasMat * cascadeViewProjMat[cascadeIndex]) * vec4(inPos, 1.0);
}

// Debuging cascades
vec3 ColorCascades(uint cascadeIndex) {
  vec3 color;

  if (cascadeIndex == 0) {
    color.rgb = vec3(1.0f, 0.25f, 0.25f);
  } else if (cascadeIndex == 1) {
    color.rgb = vec3(0.25f, 1.0f, 0.25f);
  } else if (cascadeIndex == 2) {
    color.rgb = vec3(0.25f, 0.25f, 1.0f);
  } else if (cascadeIndex == 3) {
    color.rgb = vec3(1.0f, 1.0f, 0.25f);
  }

  return color;
}