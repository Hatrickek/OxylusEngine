#define PI 3.1415926535897932384626433832795
#define TwoPI = 2 * PI;
#define Epsilon = 0.00001;

struct Vertex {
  vec3 Position;
  int _pad;
  vec3 Normal;
  int _pad2;
  vec2 UV;
  vec2 _pad3;
  vec4 Tangent;
  vec4 Color;
  vec4 Joint0;
  vec4 Weight0;
  float _pad4;
};

struct CameraData {
  vec4 Position;
  mat4 ProjectionMatrix;
  mat4 ViewMatrix;
};

#define DIRECTIONAL_LIGHT 0
#define POINT_LIGHT 1
#define SPOT_LIGHT 2

#define SHADOW_MAP_CASCADE_COUNT 4

struct Light {
  vec4 PositionIntensity; // w: intensity
  vec4 ColorRadius;       // w: radius
  vec4 RotationType;      // w: type
};

struct SceneData {
  int NumLights;

  mat4 CascadeViewProjections[4];
  vec4 CascadeSplits;
  vec4 ScissorNormalized;

  ivec2 ScreenSize;
  bool EnableGTAO;

  int PrefilteredCubeMapIndex;
  int IrradianceMapIndex;
  int BRDFLUTIndex;

  int SkyTransmittanceLutIndex;
  int SkyMultiscatterLutIndex;
  int ShadowArrayIndex;
  int GTAOIndex;
};

float sq(float x) {
    return x * x;
}

float pow5(float x) {
    float x2 = x * x;
    return x2 * x2 * x;
}

float max3(const vec3 v) {
    return max(v.x, max(v.y, v.z));
}

vec3 DeGamma(vec3 colour) {
    return pow(colour, vec3(GAMMA));
}

vec4 DeGamma(vec4 colour) {
    return vec4(DeGamma(colour.xyz), colour.w);
}

vec3 Gamma(vec3 colour) {
    return pow(colour, vec3(1.0f / GAMMA));
}

float saturate(float value) {
    return clamp(value, 0.0f, 1.0f);
}

vec2 saturate(vec2 colour) {
    return clamp(colour, 0.0, 1.0);
}

vec3 saturate(vec3 colour) {
    return clamp(colour, 0.0, 1.0);
}

float LinearizeDepth(const float screenDepth, float DepthUnpackX, float DepthUnpackY) {
    float depthLinearizeMul = DepthUnpackX;
    float depthLinearizeAdd = DepthUnpackY;
    return depthLinearizeMul / (depthLinearizeAdd - screenDepth);
}

float interleavedGradientNoise(highp vec2 w) {
    const vec3 m = vec3(0.06711056, 0.00583715, 52.9829189);
    return fract(m.z * fract(dot(w, m.xy)));
}

highp vec4 mulMat4x4Float3(const highp mat4 m, const highp vec3 v) {
    return v.x * m[0] + (v.y * m[1] + (v.z * m[2] + m[3]));
}
