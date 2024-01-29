#pragma once

#include "RendererConfig.h"
#include "RenderPipeline.h"

#include "PBR/DirectShadowPass.h"
#include "PBR/GTAO/XeGTAO.h"

namespace Oxylus {
struct ProbeChangeEvent;
struct SkyboxLoadEvent;
constexpr auto MAX_NUM_LIGHTS = 1000;
constexpr auto MAX_NUM_MESHES = 1000;
constexpr auto MAX_NUM_LIGHTS_PER_TILE = 128;
constexpr auto MAX_NUM_FRUSTUMS = 20000;
constexpr auto PIXELS_PER_TILE = 16;
constexpr auto TILES_PER_THREADGROUP = 16;

class DefaultRenderPipeline : public RenderPipeline {
public:
  explicit DefaultRenderPipeline(const std::string& name)
    : RenderPipeline(name) {}

  ~DefaultRenderPipeline() override = default;

  void init(vuk::Allocator& allocator) override;
  void load_pipelines(vuk::Allocator& allocator);
  void shutdown() override;

  Scope<vuk::Future> on_render(vuk::Allocator& frame_allocator, const vuk::Future& target, vuk::Dimension3D dim) override;

  void on_dispatcher_events(EventDispatcher& dispatcher) override;
  void on_register_render_object(const MeshComponent& render_object) override;
  void on_register_light(const TransformComponent& transform, const LightComponent& light) override;
  void on_register_camera(Camera* camera) override;

private:
  Camera* current_camera = nullptr;

  bool initalized = false;

  // scene cubemap textures
  static constexpr auto CUBE_MAP_INDEX = 0;
  static constexpr auto PREFILTERED_CUBE_MAP_INDEX = 1;
  static constexpr auto IRRADIANCE_MAP_INDEX = 2;
  static constexpr auto SKY_ENVMAP_INDEX = 3;

  // scene textures
  static constexpr auto PBR_IMAGE_INDEX = 0;
  static constexpr auto NORMAL_IMAGE_INDEX = 1;
  static constexpr auto DEPTH_IMAGE_INDEX = 2;
  static constexpr auto BRDFLUT_INDEX = 3;
  static constexpr auto SKY_TRANSMITTANCE_LUT_INDEX = 4;
  static constexpr auto SKY_MULTISCATTER_LUT_INDEX = 5;

  // scene array textures
  static constexpr auto SHADOW_ARRAY_INDEX = 0;

  // buffers and buffer/image combined indices
  static constexpr auto LIGHTS_BUFFER_INDEX = 0;
  static constexpr auto MATERIALS_BUFFER_INDEX = 1;
  static constexpr auto GTAO_BUFFER_IMAGE_INDEX = 3;
  static constexpr auto SSR_BUFFER_IMAGE_INDEX = 6;

  // GPU Buffer
  struct LightData {
    Vec3 position = {};
    float intensity = 0.0f;
    Vec3 color = {};
    float radius = 0.0f;
    Vec3 rotation = {};
    uint32_t type = 0;
  };

  struct Lights {
    std::vector<LightData> lights;
  };

  struct CameraData {
    Vec4 position;
    Mat4 projection_matrix;
    Mat4 view_matrix;
    Mat4 inv_projection_view_matrix;

    float near;
    float far;
    float fov;
    float _pad = 0;
  };

  // GPU Buffer
  struct SceneData {
    int num_lights;
    float grid_max_distance;
    IVec2 screen_size;

    Vec3 sun_direction;
    int _pad2;
    Vec4 sun_color; // pre-multipled with intensity

    Mat4 cubemap_view_projections[6];

    Mat4 cascade_view_projections[4];
    Vec4 cascade_splits;
    Vec4 scissor_normalized;

    struct Indices {
      int pbr_image_index;
      int normal_image_index;
      int depth_image_index;
      int _pad;

      int cube_map_index;
      int prefiltered_cube_map_index;
      int irradiance_map_index;
      int brdflut_index;

      int sky_envmap_index;
      int sky_transmittance_lut_index;
      int sky_multiscatter_lut_index;
      int shadow_array_index;

      int gtao_buffer_image_index;
      int ssr_buffer_image_index;
      int lights_buffer_index;
      int materials_buffer_index;
    } indices;

    struct PostProcessingData {
      int tonemapper = RendererConfig::TONEMAP_ACES;
      float exposure = 1.0f;
      float gamma = 2.5f;
      int _pad;

      int enable_bloom = 1;
      int enable_ssr = 1;
      int enable_gtao = 1;
      int _pad2;

      Vec4 vignette_color = Vec4(0.0f, 0.0f, 0.0f, 0.25f); // rgb: color, a: intensity
      Vec4 vignette_offset = Vec4(0.0f, 0.0f, 0.0f, 0.0f); // xy: offset, z: useMask, w: enable effect
      Vec2 film_grain = {};                                // x: enable, y: amount
      Vec2 chromatic_aberration = {};                      // x: enable, y: amount
      Vec2 sharpen = {};                                   // x: enable, y: amount
    } post_processing_data;

    Vec2 _pad;
  } scene_data;

  struct ShaderPC {
    Mat4 model_matrix;
    uint64_t vertex_buffer_ptr;
    uint32_t material_index;
  };

  struct SSRData {
    int samples = 64;
    int binary_search_samples = 16;
    float max_dist = 100.0f;
    int _pad;
  } ssr_data;

  vuk::Unique<vuk::PersistentDescriptorSet> descriptor_set_00;
  vuk::Unique<vuk::PersistentDescriptorSet> descriptor_set_01;

  vuk::Texture pbr_texture;
  vuk::Texture normal_texture;
  vuk::Texture depth_texture;

  vuk::Texture sky_transmittance_lut;
  vuk::Texture sky_multiscatter_lut;
  vuk::Texture sky_envmap_texture;
  vuk::Texture gtao_final_texture;
  vuk::Texture ssr_texture;
  vuk::Texture sun_shadow_texture;

  XeGTAO::GTAOConstants gtao_constants = {};
  XeGTAO::GTAOSettings gtao_settings = {};

  // PBR Resources
  Ref<TextureAsset> cube_map = nullptr;
  vuk::Texture brdf_texture;
  vuk::Texture irradiance_texture;
  vuk::Texture prefiltered_texture;

  // Mesh
  std::vector<MeshComponent> mesh_draw_list;
  bool m_should_merge_render_objects = false;
  vuk::Buffer m_merged_index_buffer;
  vuk::Buffer m_merged_vertex_buffer;
  Ref<Mesh> m_quad = nullptr;
  Ref<Mesh> m_cube = nullptr;
  Ref<Camera> default_camera;

  struct LightChangeEvent {};

  DirectShadowPass::DirectShadowUB direct_shadow_ub = {};
  std::vector<LightData> scene_lights = {};
  LightData* dir_light_data = nullptr;
  EventDispatcher light_buffer_dispatcher;
  bool is_cube_map_pipeline = false;

  void commit_descriptor_sets(vuk::Allocator& allocator);
  void create_static_textures(vuk::Allocator& allocator);
  void create_dynamic_textures(vuk::Allocator& allocator, const vuk::Dimension3D& dim);
  void create_descriptor_sets(vuk::Allocator& allocator, vuk::Context& ctx);

  void update_skybox(const SkyboxLoadEvent& e);
  void generate_prefilter(vuk::Allocator& allocator);
  void update_parameters(ProbeChangeEvent& e);

  void sky_envmap_pass(const Ref<vuk::RenderGraph>& rg);
  void sky_view_lut_pass(const Ref<vuk::RenderGraph>& rg);
  void sky_transmittance_pass(const Ref<vuk::RenderGraph>& rg);
  void sky_multiscatter_pass(const Ref<vuk::RenderGraph>& rg);
  void depth_pre_pass(const Ref<vuk::RenderGraph>& rg);
  void geometry_pass(const Ref<vuk::RenderGraph>& rg);
  void apply_fxaa(vuk::RenderGraph* rg, vuk::Name src, vuk::Name dst, vuk::Buffer& fxaa_buffer);
  void cascaded_shadow_pass(const Ref<vuk::RenderGraph>& rg);
  void gtao_pass(vuk::Allocator& frame_allocator, const Ref<vuk::RenderGraph>& rg);
  void ssr_pass(const Ref<vuk::RenderGraph>& rg);
  void bloom_pass(const Ref<vuk::RenderGraph>& rg);
  void apply_grid(vuk::RenderGraph* rg, const vuk::Name dst, const vuk::Name depth_image_name);
  void debug_pass(const Ref<vuk::RenderGraph>& rg, vuk::Name dst, const char* depth, vuk::Allocator& frame_allocator) const;
};
}
