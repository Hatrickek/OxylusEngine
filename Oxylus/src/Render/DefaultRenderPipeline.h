#pragma once

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

  void init(Scene* scene) override;
  void shutdown() override;
  void update(Scene* scene) override;
  Scope<vuk::Future> on_render(vuk::Allocator& frame_allocator, const vuk::Future& target, vuk::Dimension3D dim) override;
  void on_dispatcher_events(EventDispatcher& dispatcher) override;

private:
  struct RendererContext {
    //Camera
    Camera* current_camera = nullptr;
  } m_renderer_context;

  struct RendererData {
    struct UBOVS {
      Mat4 projection;
      Mat4 view;
      Vec3 cam_pos;
    } ubo_vs;

    struct FinalPassData {
      int tonemapper = RendererConfig::TONEMAP_ACES;          // 0- Aces 1- Uncharted 2-Filmic 3- Reinhard
      float exposure = 1.0f;
      float gamma = 2.5f;
      int enable_ssao = 1;
      int enable_bloom = 1;
      int enable_ssr = 1;
      Vec2 _pad{};
      Vec4 vignette_color = Vec4(0.0f, 0.0f, 0.0f, 0.25f);     // rgb: color, a: intensity
      Vec4 vignette_offset = Vec4(0.0f, 0.0f, 0.0f, 0.0f);     // xy: offset, z: useMask, w: enable effect
      Vec2 film_grain = {};                                    // x: enable, y: amount
      Vec2 chromatic_aberration = {};                          // x: enable, y: amount
      Vec2 sharpen = {};                                      // x: enable, y: amount
    } final_pass_data;
  } m_renderer_data = {};

  struct RendererResources {
    Ref<TextureAsset> cube_map;
    Ref<vuk::Texture> lut_brdf;
    Ref<vuk::Texture> irradiance_cube;
    Ref<vuk::Texture> prefiltered_cube;
  } m_resources;

  XeGTAO::GTAOConstants gtao_constants = {};
  XeGTAO::GTAOSettings gtao_settings = {};

  Scene* m_scene = nullptr;

  // PBR Resources
  vuk::Unique<vuk::Image> brdf_image;
  vuk::Unique<vuk::Image> irradiance_image;
  vuk::Unique<vuk::Image> prefiltered_image;

  // Mesh
  std::vector<VulkanRenderer::MeshData> mesh_draw_list;
  std::vector<uint32_t> transparent_mesh_draw_list;

  struct LightChangeEvent { };

  DirectShadowPass::DirectShadowUB direct_shadow_ub = {};
  std::vector<VulkanRenderer::LightingData> point_lights_data = {};
  std::vector<VulkanRenderer::LightingData> dir_lights_data = {};
  std::vector<VulkanRenderer::LightingData> spot_lights_data = {};
  EventDispatcher light_buffer_dispatcher;
  Ref<Mesh> skybox_cube = nullptr;

  void init_render_graph();
  void update_skybox(const SkyboxLoadEvent& e);
  void generate_prefilter();
  void update_parameters(ProbeChangeEvent& e);
  void update_final_pass_data(RendererConfig::ConfigChangeEvent& e);

  void depth_pre_pass(const Ref<vuk::RenderGraph>& rg, vuk::Buffer& vs_buffer, const std::unordered_map<uint32_t, uint32_t>&, vuk::Buffer& mat_buffer);
  void geomerty_pass(const Ref<vuk::RenderGraph>& rg, vuk::Buffer& vs_buffer, const std::unordered_map<uint32_t, uint32_t>&, vuk::Buffer& mat_buffer, vuk::Buffer& shadow_buffer, vuk::Buffer& point_lights_buffer, vuk::Buffer pbr_buffer);
  vuk::Future apply_fxaa(vuk::Future source, vuk::Future dst);
  void cascaded_shadow_pass(const Ref<vuk::RenderGraph>& rg, vuk::Buffer& shadow_buffer);
  void gtao_pass(vuk::Allocator& frame_allocator, const Ref<vuk::RenderGraph>& rg);
  void ssr_pass(vuk::Allocator& frame_allocator, const Ref<vuk::RenderGraph>& rg, VulkanContext* vk_context, vuk::Buffer& vs_buffer) const;
};
}
