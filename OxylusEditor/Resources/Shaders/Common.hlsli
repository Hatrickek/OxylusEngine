#ifndef COMMON_HLSLI
#define COMMON_HLSLI

#define UINT32_MAX 4294967295
#define INVALID_ID UINT32_MAX
#define PI 3.1415926535897932384626433832795
#define TwoPI 2 * PI
#define EPSILON 0.00001

#define MEDIUMP_FLT_MAX 65504.0
#define saturateMediump(x) min(x, MEDIUMP_FLT_MAX)
#define sqr(a) ((a) * (a))
#define pow5(x) pow(x, 5)

struct Vertex {
  float3 position : POSITION;
  int _pad : PAD0;
  float3 normal : NORMAL;
  int _pad2 : PAD1;
  float2 uv : TEXCOORD0;
  float2 _pad3 : PAD2;
  float4 tangent : TEXCOORD1;
  float4 color : TEXCOORD2;
  float4 joint0 : TEXCOORD3;
  float4 weight0 : TEXCOORD4;
};

struct VertexOutput {
#ifdef USE_POSITION
  float4 position : SV_POSITION;
#endif
#ifdef USE_WORLD_POS
  float3 world_pos : WORLD_POSITION;
#endif
#ifdef USE_NORMAL
  float3 normal : NORMAL;
#endif
#ifdef USE_UV
  float2 uv : TEXCOORD;
#endif
#ifdef USE_VIEWPOS
  float3 view_pos : VIEW_POS;
#endif
#ifdef USE_TANGENT
  float4 tangent : TANGENT;
#endif
#ifdef USE_VIEWPORT
  uint vp_index : SV_ViewportArrayIndex;
#endif
  uint draw_index : DrawIndex;
};

struct PushConst {
  uint64_t vertex_buffer_ptr;
  uint instance_offset;
  uint material_index;
};

struct MeshInstance {
  float4x4 transform;

  void init() { transform = 0; }
};

struct MeshInstancePointer {
  uint data;

  void init() { data = 0; }
  void Create(uint instance_index, uint camera_index = 0, float dither = 0) {
    data = 0;
    data |= instance_index & 0xFFFFFF;
    data |= (camera_index & 0xF) << 24u;
    data |= (uint(dither * 15.0f) & 0xF) << 28u;
  }
  uint GetInstanceIndex() { return data & 0xFFFFFF; }
  uint GetCameraIndex() { return (data >> 24u) & 0xF; }
  float GetDither() { return float((data >> 28u) & 0xF) / 15.0f; }
};

struct ShaderEntity {
  float4x4 transform;
};

struct CameraData {
  float4 position;
  float4x4 projection_matrix;
  float4x4 inv_projection_matrix;
  float4x4 view_matrix;
  float4x4 inv_view_matrix;
  float4x4 inv_view_projection_matrix;
  float4x4 projection_view_matrix;
  float3 up;
  float near_clip;
  float3 forward;
  float far_clip;
  float3 right;
  float fov;
  float3 _pad;
  uint output_index;
};

struct CameraCB {
  CameraData camera_data[16];
};

#define DIRECTIONAL_LIGHT 0
#define POINT_LIGHT 1
#define SPOT_LIGHT 2

#define SHADOW_MAP_CASCADE_COUNT 4

struct Light {
  float3 position;
  float intensity;
  float3 color;
  float radius;
  float3 rotation;
  uint type;
  float3 direction;
  uint cascade_count;
  float4 shadow_atlas_mul_add;
  float3 _pad;
  uint matrix_index;
};

struct SceneData {
  int num_lights;
  float grid_max_distance;
  int2 screen_size;

  uint2 pad0;
  uint2 shadow_atlas_res;

  float3 sun_direction;
  int _pad1;

  float4 sun_color; // pre-multipled with intensity

  struct Indices {
    int pbr_image_index;
    int normal_image_index;
    int depth_image_index;
    int mesh_instance_buffer_index;

    int entites_buffer_index;
    int materials_buffer_index;
    int lights_buffer_index;
    int _pad0;

    int sky_env_map_index;
    int sky_transmittance_lut_index;
    int sky_multiscatter_lut_index;
    int _pad1;

    int shadow_array_index;
    int gtao_buffer_image_index;
    int2 _pad2;
  } indices_;

  struct PostProcessingData {
    int tonemapper;
    float exposure;
    float gamma;
    int _pad;

    int enable_bloom;
    int enable_ssr;
    int enable_gtao;
    int _pad2;

    float4 vignette_color;       // rgb: color, a: intensity
    float4 vignette_offset;      // xy: offset, z: useMask, w: enable effect
    float2 film_grain;           // x: enable, y: amount
    float2 chromatic_aberration; // x: enable, y: amount
    float2 sharpen;              // x: enable, y: amount
    float2 _pad3;
  } post_processing_data;
};

struct SSRData {
  int Samples;
  int BinarySearchSamples;
  float MaxDist;
  int _pad;
};

bool is_nan(float3 vec) {
  return (asuint(vec.x) & 0x7fffffff) > 0x7f800000 || (asuint(vec.y) & 0x7fffffff) > 0x7f800000 || (asuint(vec.z) & 0x7fffffff) > 0x7f800000;
}

inline bool is_saturated(float a) { return a == saturate(a); }
inline bool is_saturated(float2 a) { return all(a == saturate(a)); }
inline bool is_saturated(float3 a) { return all(a == saturate(a)); }
inline bool is_saturated(float4 a) { return all(a == saturate(a)); }

inline float3 clipspace_to_uv(in float3 clipspace) { return clipspace * float3(0.5, 0.5, 0.5) + 0.5; }

float2x2 inverse(float2x2 mat) {
  float2x2 m;
  m[0][0] = mat[1][1];
  m[0][1] = -mat[0][1];
  m[1][0] = -mat[1][0];
  m[1][1] = mat[0][0];

  return mul((1.0f / mat[0][0] * mat[1][1] - mat[0][1] * mat[1][0]), m);
}
#endif
