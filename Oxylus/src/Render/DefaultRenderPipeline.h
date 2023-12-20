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
    : RenderPipeline(name) { }

  ~DefaultRenderPipeline() override = default;

  void init(vuk::Allocator& allocator) override;
  void shutdown() override;

  Scope<vuk::Future> on_render(vuk::Allocator& frame_allocator, const vuk::Future& target, vuk::Dimension3D dim) override;

  void on_dispatcher_events(EventDispatcher& dispatcher) override;
  void on_register_render_object(const MeshComponent& render_object) override;
  void on_register_light(const LightingData& lighting_data, LightComponent::LightType light_type) override;
  void on_register_camera(Camera* camera) override;

private:
  struct RendererContext {
    //Camera
    Camera* current_camera = nullptr;
  } m_renderer_context;

  struct RendererData {
    // TODO: make a global camera data buffer in Camera.h

    struct UBOVS {
      Mat4 projection;
      Mat4 view;
      Vec3 cam_pos;
    } ubo_vs;

    struct FinalPassData {
      int tonemapper = RendererConfig::TONEMAP_ACES;
      float exposure = 1.0f;
      float gamma = 2.5f;
      int enable_bloom = 1;
      int enable_ssr = 1;
      Vec4 vignette_color = Vec4(0.0f, 0.0f, 0.0f, 0.25f); // rgb: color, a: intensity
      Vec4 vignette_offset = Vec4(0.0f, 0.0f, 0.0f, 0.0f); // xy: offset, z: useMask, w: enable effect
      Vec2 film_grain = {};                                // x: enable, y: amount
      Vec2 chromatic_aberration = {};                      // x: enable, y: amount
      Vec2 sharpen = {};                                   // x: enable, y: amount
      Vec2 _pad;
    } final_pass_data;

    struct Atmosphere {
      constexpr static float kuM = 1e-6f;
      constexpr static float kKM = 1e3f;

      Vec3 rayleigh_scatter_val = {5.802 * kuM, 13.558 * kuM, 33.1 * kuM};
      float rayleigh_density = 8 * kKM;

      float planet_radius = 6360.f * kKM;
      float atmos_radius = 6460.f * kKM;

      float mie_scatter_val = 3.996f * kuM;
      float mie_absorption_val = 4.4f * kuM;
      float mie_density = 1.2f * kKM;
      float mie_asymmetry = 0.8f;

      float ozone_height = 25 * kKM;
      float ozone_thickness = 15 * kKM;
      Vec3 ozone_absorption = {0.650 * kuM, 1.881 * kuM, 0.085 * kuM};

      float _padding;
    } m_atmosphere;

    struct EyeViewData {
      Vec3 camera_position = {};
      float _pad;
      Vec3 sun_direction = {};
      float _pad2;
      Vec3 sun_color = {}; // pre-multipled with intensity
      float _pad3;
    } eye_view_data;

    struct SunData {
      Vec3 sun_dir = {};
      float sun_radius = 0.3f;
      Vec4 frustum[4] = {};
    } sun_data;
  } m_renderer_data = {};

  XeGTAO::GTAOConstants gtao_constants = {};
  XeGTAO::GTAOSettings gtao_settings = {};

  // PBR Resources
  Ref<TextureAsset> cube_map = nullptr;
  vuk::Unique<vuk::Image> brdf_image;
  vuk::Unique<vuk::Image> irradiance_image;
  vuk::Unique<vuk::Image> prefiltered_image;

  // Mesh
  std::vector<MeshComponent> mesh_draw_list;
  bool m_should_merge_render_objects = false;
  vuk::Buffer m_merged_index_buffer;
  vuk::Buffer m_merged_vertex_buffer;
  Ref<Mesh> m_quad = nullptr;
  Ref<Mesh> m_cube = nullptr;
  Ref<Camera> default_camera;

  struct LightChangeEvent { };

  DirectShadowPass::DirectShadowUB direct_shadow_ub = {};
  std::vector<LightingData> scene_lights_data = {};
  EventDispatcher light_buffer_dispatcher;
  bool is_cube_map_pipeline = false;

  void update_skybox(const SkyboxLoadEvent& e);
  void generate_prefilter(vuk::Allocator& allocator);
  void update_parameters(ProbeChangeEvent& e);

  void sky_view_lut_pass(const Ref<vuk::RenderGraph>& rg);
  void sky_transmittance_pass(const Ref<vuk::RenderGraph>& rg);
  void sky_multiscatter_pass(const Ref<vuk::RenderGraph>& rg);
  void depth_pre_pass(const Ref<vuk::RenderGraph>& rg, vuk::Buffer& vs_buffer, const std::unordered_map<uint32_t, uint32_t>&, vuk::Buffer& mat_buffer) const;
  void geomerty_pass(const Ref<vuk::RenderGraph>& rg, vuk::Allocator& frame_allocator, vuk::Buffer& vs_buffer, const std::unordered_map<uint32_t, uint32_t>&, vuk::Buffer& mat_buffer, vuk::Buffer& shadow_buffer, vuk::Buffer& scene_lights_buffer);
  void apply_fxaa(vuk::RenderGraph* rg, vuk::Name src, vuk::Name dst, vuk::Buffer& fxaa_buffer);
  void cascaded_shadow_pass(const Ref<vuk::RenderGraph>& rg, vuk::Buffer& shadow_buffer) const;
  void gtao_pass(vuk::Allocator& frame_allocator, const Ref<vuk::RenderGraph>& rg);
  void ssr_pass(vuk::Allocator& frame_allocator, const Ref<vuk::RenderGraph>& rg, vuk::Buffer& vs_buffer) const;
  void bloom_pass(const Ref<vuk::RenderGraph>& rg);
  void apply_grid(vuk::RenderGraph* rg, vuk::Name dst, vuk::Name depth_image, vuk::Allocator& frame_allocator) const;
  void debug_pass(const Ref<vuk::RenderGraph>& rg, vuk::Name dst, const char* depth, vuk::Allocator& frame_allocator) const;
};
}
