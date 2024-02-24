#pragma once

#include "RendererConfig.h"
#include "RenderPipeline.h"

#include "PBR/DirectShadowPass.h"
#include "PBR/GTAO/XeGTAO.h"

namespace Ox {
struct SkyboxLoadEvent;

class DefaultRenderPipeline : public RenderPipeline {
public:
  explicit DefaultRenderPipeline(const std::string& name)
    : RenderPipeline(name) {}

  ~DefaultRenderPipeline() override = default;

  void init(vuk::Allocator& allocator) override;
  void load_pipelines(vuk::Allocator& allocator);
  void shutdown() override;

  Unique<vuk::Future> on_render(vuk::Allocator& frame_allocator, const vuk::Future& target, vuk::Dimension3D dim) override;

  void on_update(Scene* scene) override;

  void on_dispatcher_events(EventDispatcher& dispatcher) override;
  void on_register_render_object(const MeshComponent& render_object) override;
  void on_register_light(const TransformComponent& transform, const LightComponent& light) override;
  void on_register_camera(Camera* camera) override;

private:
  Camera* current_camera = nullptr;

  bool initalized = false;

  // scene cubemap textures
  static constexpr auto SKY_ENVMAP_INDEX = 0;

  // scene textures
  static constexpr auto PBR_IMAGE_INDEX = 0;
  static constexpr auto NORMAL_IMAGE_INDEX = 1;
  static constexpr auto DEPTH_IMAGE_INDEX = 2;
  static constexpr auto SKY_TRANSMITTANCE_LUT_INDEX = 3;
  static constexpr auto SKY_MULTISCATTER_LUT_INDEX = 4;

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
    Mat4 inv_projection_matrix;
    Mat4 view_matrix;
    Mat4 inv_view_matrix;
    Mat4 inv_projection_view_matrix;

    float near_clip;
    float far_clip;
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
  Shared<TextureAsset> cube_map = nullptr;
  vuk::Texture brdf_texture;
  vuk::Texture irradiance_texture;
  vuk::Texture prefiltered_texture;

  enum Filter {
    // Include nothing:
    FILTER_NONE = 0,

    // Object filtering types:
    FILTER_OPAQUE          = 1 << 0,
    FILTER_TRANSPARENT     = 1 << 1,
    FILTER_WATER           = 1 << 2,
    FILTER_NAVIGATION_MESH = 1 << 3,
    FILTER_OBJECT_ALL      = FILTER_OPAQUE | FILTER_TRANSPARENT | FILTER_WATER | FILTER_NAVIGATION_MESH,

    // Include everything:
    FILTER_ALL = ~0,
  };

  enum RenderFlags {
    RENDER_FLAGS_NONE = 0,

    // Use push constant
    RENDER_FLAGS_USE_PC = 1 << 0,

    RENDER_FLAGS_PUSH_MATERIAL_INDEX = 1 << 1,

    RENDER_FLAGS_PC_BIND_FRG = 1 << 2,
  };

  struct RenderBatch {
    uint32_t mesh_index;
    uint32_t instance_index;
    uint16_t distance;
    uint16_t camera_mask;
    uint32_t sort_bits; // an additional bitmask for sorting only, it should be used to reduce pipeline changes

    void create(const uint32_t p_mesh_index, const uint32_t p_instance_index, const float p_distance, const uint32_t p_sort_bits, const uint16_t p_camera_mask = 0xFFFF) {
      this->mesh_index = p_mesh_index;
      this->instance_index = p_instance_index;
      this->distance = uint16_t(glm::floatBitsToUint(p_distance));
      this->sort_bits = p_sort_bits;
      this->camera_mask = p_camera_mask;
    }

    float get_distance() const { return glm::uintBitsToFloat(distance); }
    uint32_t get_mesh_index() const { return mesh_index; }
    uint32_t get_instance_index() const { return instance_index; }

    // opaque sorting
    // Priority is set to mesh index to have more instancing
    // distance is second priority (front to back Z-buffering)
    constexpr bool operator<(const RenderBatch& other) const {
      union SortKey {
        struct {
          // The order of members is important here, it means the sort priority (low to high)!
          uint64_t distance : 16;
          uint64_t mesh_index : 16;
          uint64_t sort_bits : 32;
        } bits;

        uint64_t value;
      };
      static_assert(sizeof(SortKey) == sizeof(uint64_t));
      const SortKey a = {.bits = {.distance = distance, .mesh_index = mesh_index, .sort_bits = sort_bits}};
      const SortKey b = {.bits = {.distance = other.distance, .mesh_index = other.mesh_index, .sort_bits = other.sort_bits}};
      return a.value < b.value;
    }

    // transparent sorting
    // Priority is distance for correct alpha blending (back to front rendering)
    // mesh index is second priority for instancing
    constexpr bool operator>(const RenderBatch& other) const {
      union SortKey {
        struct {
          // The order of members is important here, it means the sort priority (low to high)!
          uint64_t mesh_index : 16;
          uint64_t sort_bits : 32;
          uint64_t distance : 16;
        } bits;

        uint64_t value;
      };
      static_assert(sizeof(SortKey) == sizeof(uint64_t));
      const SortKey a = {.bits = {.mesh_index = mesh_index, .sort_bits = sort_bits, .distance = distance}};
      const SortKey b = {.bits = {.mesh_index = other.mesh_index, .sort_bits = other.sort_bits, .distance = other.distance}};
      return a.value > b.value;
    }
  };

  struct RenderQueue {
    std::vector<RenderBatch> batches = {};

    void clear() {
      batches.clear();
    }

    void add(const uint32_t mesh_index, const uint32_t instance_index, const float distance, const uint32_t sort_bits, const uint16_t camera_mask = 0xFFFF) {
      batches.emplace_back().create(mesh_index, instance_index, distance, sort_bits, camera_mask);
    }

    void sort_transparent() {
      OX_SCOPED_ZONE;
      std::sort(batches.begin(), batches.end(), std::greater<RenderBatch>());
    }

    void sort_opaque() {
      OX_SCOPED_ZONE;
      std::sort(batches.begin(), batches.end(), std::less<RenderBatch>());
    }

    bool empty() const {
      return batches.empty();
    }

    size_t size() const {
      return batches.size();
    }
  };

  std::vector<MeshComponent> mesh_component_list;
  RenderQueue render_queue;
  vuk::Buffer merged_index_buffer;
  vuk::Buffer merged_vertex_buffer;
  Shared<Mesh> m_quad = nullptr;
  Shared<Mesh> m_cube = nullptr;
  Shared<Camera> default_camera;

  DirectShadowPass::DirectShadowUB direct_shadow_ub = {};
  std::vector<std::pair<LightData, LightComponent>> scene_lights = {};
  std::pair<LightData, LightComponent>* dir_light_data = nullptr;
  LightComponent* dir_light_component = nullptr;

  void commit_descriptor_sets(vuk::Allocator& allocator);
  void create_static_textures(vuk::Allocator& allocator);
  void create_dynamic_textures(vuk::Allocator& allocator, const vuk::Dimension3D& dim);
  void create_descriptor_sets(vuk::Allocator& allocator, vuk::Context& ctx);

  void update_skybox(const SkyboxLoadEvent& e);
  void generate_prefilter(vuk::Allocator& allocator);

  void sky_envmap_pass(const Shared<vuk::RenderGraph>& rg);
  void sky_view_lut_pass(const Shared<vuk::RenderGraph>& rg);
  void sky_transmittance_pass(const Shared<vuk::RenderGraph>& rg);
  void sky_multiscatter_pass(const Shared<vuk::RenderGraph>& rg);
  void depth_pre_pass(const Shared<vuk::RenderGraph>& rg);
  void render_meshes(const RenderQueue& render_queue, vuk::CommandBuffer& command_buffer, uint32_t filter, uint32_t flags, uint32_t push_index) const;
  void geometry_pass(const Shared<vuk::RenderGraph>& rg);
  void apply_fxaa(vuk::RenderGraph* rg, vuk::Name src, vuk::Name dst, vuk::Buffer& fxaa_buffer);
  void cascaded_shadow_pass(const Shared<vuk::RenderGraph>& rg);
  void gtao_pass(vuk::Allocator& frame_allocator, const Shared<vuk::RenderGraph>& rg);
  void ssr_pass(const Shared<vuk::RenderGraph>& rg);
  void bloom_pass(const Shared<vuk::RenderGraph>& rg);
  void apply_grid(vuk::RenderGraph* rg, const vuk::Name dst, const vuk::Name depth_image_name);
  void debug_pass(const Shared<vuk::RenderGraph>& rg, vuk::Name dst, const char* depth, vuk::Allocator& frame_allocator) const;
};
}
