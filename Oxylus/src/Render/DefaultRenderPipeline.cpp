#include "DefaultRenderPipeline.h"

#include <glm/gtc/type_ptr.inl>
#include <vuk/Partials.hpp>

#include "DebugRenderer.h"
#include "RendererCommon.h"
#include "SceneRendererEvents.h"

#include "Core/Application.h"
#include "Assets/AssetManager.h"
#include "Core/Entity.h"
#include "Core/Resources.h"
#include "PBR/DirectShadowPass.h"
#include "PBR/Prefilter.h"

#include "Utils/CVars.h"
#include "Utils/FileUtils.h"
#include "Utils/Profiler.h"
#include "Vulkan/VukUtils.h"
#include "Vulkan/VulkanContext.h"

#include "Vulkan/Renderer.h"

namespace Oxylus {
void DefaultRenderPipeline::init() {
  m_quad = RendererCommon::generate_quad();
  m_cube = RendererCommon::generate_cube();

  // Lights data
  point_lights_data.reserve(MAX_NUM_LIGHTS);

  // Mesh data
  mesh_draw_list.reserve(MAX_NUM_MESHES);
  transparent_mesh_draw_list.reserve(MAX_NUM_MESHES);

  // TODO: don't load a default hdr texture / load a light blue colored texture
  cube_map = create_ref<TextureAsset>();
  cube_map = AssetManager::get_texture_asset({.path = Resources::get_resources_path("HDRs/table_mountain_2_puresky_2k.hdr"), .format = vuk::Format::eR8G8B8A8Srgb});
  generate_prefilter();

  init_render_graph();
}

void DefaultRenderPipeline::on_dispatcher_events(EventDispatcher& dispatcher) {
  dispatcher.sink<SkyboxLoadEvent>().connect<&DefaultRenderPipeline::update_skybox>(*this);
  dispatcher.sink<ProbeChangeEvent>().connect<&DefaultRenderPipeline::update_parameters>(*this);
}

void DefaultRenderPipeline::on_register_render_object(const MeshData& render_object) {
  mesh_draw_list.emplace_back(render_object);
}

void DefaultRenderPipeline::on_register_light(const LightingData& lighting_data, LightComponent::LightType light_type) {
  switch (light_type) {
    case LightComponent::LightType::Directional: {
      dir_lights_data.emplace_back(lighting_data);
      break;
    }
    case LightComponent::LightType::Point: {
      point_lights_data.emplace_back(lighting_data);
      break;
    }
    case LightComponent::LightType::Spot: {
      spot_lights_data.emplace_back(lighting_data);
      break;
    }
  }

  light_buffer_dispatcher.trigger(LightChangeEvent{});
}

void DefaultRenderPipeline::on_register_camera(Camera* camera) {
  m_renderer_context.current_camera = camera;
}

void DefaultRenderPipeline::shutdown() { }

void DefaultRenderPipeline::init_render_graph() {
  const auto vk_context = VulkanContext::get();

  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileUtils::read_shader_file("DepthNormalPass.vert"), FileUtils::get_shader_path("DepthNormalPass.vert"));
    pci.add_glsl(FileUtils::read_shader_file("DepthNormalPass.frag"), FileUtils::get_shader_path("DepthNormalPass.frag"));
    vk_context->context->create_named_pipeline("depth_pre_pass_pipeline", pci);
  }
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileUtils::read_shader_file("DirectShadowDepthPass.vert"), "DirectShadowDepthPass.vert");
    pci.add_glsl(FileUtils::read_shader_file("DirectShadowDepthPass.frag"), "DirectShadowDepthPass.frag");
    vk_context->context->create_named_pipeline("shadow_pipeline", pci);
  }
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileUtils::read_shader_file("Skybox.vert"), "Skybox.vert");
    pci.add_glsl(FileUtils::read_shader_file("Skybox.frag"), "Skybox.frag");
    vk_context->context->create_named_pipeline("skybox_pipeline", pci);
  }
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileUtils::read_shader_file("PBRTiled.vert"), FileUtils::get_shader_path("PBRTiled.vert"));
    pci.add_glsl(FileUtils::read_shader_file("PBRTiled.frag"), FileUtils::get_shader_path("PBRTiled.frag"));
    vk_context->context->create_named_pipeline("pbr_pipeline", pci);
  }
#if 0 // TODO
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileUtils::read_shader_file("Unlit.vert"), "Unlit.vert");
    pci.add_glsl(FileUtils::read_shader_file("Unlit.frag"), "Unlit.frag");
    vk_context->context->create_named_pipeline("debug_pipeline", pci);
  }
#endif
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileUtils::read_shader_file("SSR.comp"), FileUtils::get_shader_path("SSR.comp"));
    vk_context->context->create_named_pipeline("ssr_pipeline", pci);
  }
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileUtils::read_shader_file("FullscreenPass.vert"), FileUtils::get_shader_path("FullscreenPass.vert"));
    pci.add_glsl(FileUtils::read_shader_file("FinalPass.frag"), FileUtils::get_shader_path("FinalPass.frag"));
    vk_context->context->create_named_pipeline("final_pipeline", pci);
  }
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_hlsl(FileUtils::read_shader_file("GTAO/GTAO_First.hlsl"), FileUtils::get_shader_path("GTAO/GTAO_First.hlsl"), vuk::HlslShaderStage::eCompute, "CSPrefilterDepths16x16");
    pci.define("XE_GTAO_FP32_DEPTHS", "");
    pci.define("XE_GTAO_USE_HALF_FLOAT_PRECISION", "0");
    pci.define("XE_GTAO_USE_DEFAULT_CONSTANTS", "0");
    //pci.define("XE_GTAO_COMPUTE_BENT_NORMALS", "");
    vk_context->context->create_named_pipeline("gtao_first_pipeline", pci);
  }
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_hlsl(FileUtils::read_shader_file("GTAO/GTAO_Main.hlsl"), FileUtils::get_shader_path("GTAO/GTAO_Main.hlsl"), vuk::HlslShaderStage::eCompute, "CSGTAOHigh");
    pci.define("XE_GTAO_FP32_DEPTHS", "");
    pci.define("XE_GTAO_USE_HALF_FLOAT_PRECISION", "0");
    pci.define("XE_GTAO_USE_DEFAULT_CONSTANTS", "0");
    //pci.define("XE_GTAO_COMPUTE_BENT_NORMALS", "");
    vk_context->context->create_named_pipeline("gtao_main_pipeline", pci);
  }
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_hlsl(FileUtils::read_shader_file("GTAO/GTAO_Final.hlsl"), FileUtils::get_shader_path("GTAO/GTAO_Final.hlsl"), vuk::HlslShaderStage::eCompute, "CSDenoisePass");
    pci.define("XE_GTAO_FP32_DEPTHS", "");
    pci.define("XE_GTAO_USE_HALF_FLOAT_PRECISION", "0");
    pci.define("XE_GTAO_USE_DEFAULT_CONSTANTS", "0");
    //pci.define("XE_GTAO_COMPUTE_BENT_NORMALS", "");
    vk_context->context->create_named_pipeline("gtao_denoise_pipeline", pci);
  }
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_hlsl(FileUtils::read_shader_file("GTAO/GTAO_Final.hlsl"), FileUtils::get_shader_path("GTAO/GTAO_Final.hlsl"), vuk::HlslShaderStage::eCompute, "CSDenoiseLastPass");
    pci.define("XE_GTAO_FP32_DEPTHS", "");
    pci.define("XE_GTAO_USE_HALF_FLOAT_PRECISION", "0");
    pci.define("XE_GTAO_USE_DEFAULT_CONSTANTS", "0");
    //pci.define("XE_GTAO_COMPUTE_BENT_NORMALS", "");
    vk_context->context->create_named_pipeline("gtao_final_pipeline", pci);
  }
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileUtils::read_shader_file("FullscreenPass.vert"), FileUtils::get_shader_path("FullscreenPass.vert"));
    pci.add_glsl(FileUtils::read_shader_file("FXAA.frag"), FileUtils::get_shader_path("FXAA.frag"));
    vk_context->context->create_named_pipeline("fxaa_pipeline", pci);
  }
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileUtils::read_shader_file("BloomPrefilter.comp"), FileUtils::get_shader_path("BloomPrefilter.comp"));
    vk_context->context->create_named_pipeline("bloom_prefilter_pipeline", pci);
  }
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileUtils::read_shader_file("BloomDownsample.comp"), FileUtils::get_shader_path("BloomDownsample.comp"));
    vk_context->context->create_named_pipeline("bloom_downsample_pipeline", pci);
  }
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileUtils::read_shader_file("BloomUpsample.comp"), FileUtils::get_shader_path("BloomUpsample.comp"));
    vk_context->context->create_named_pipeline("bloom_upsample_pipeline", pci);
  }
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileUtils::read_shader_file("Grid.vert"), FileUtils::get_shader_path("Grid.vert"));
    pci.add_glsl(FileUtils::read_shader_file("Grid.frag"), FileUtils::get_shader_path("Grid.frag"));
    vk_context->context->create_named_pipeline("grid_pipeline", pci);
  }
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileUtils::read_shader_file("Unlit.vert"), FileUtils::get_shader_path("Unlit.vert"));
    pci.add_glsl(FileUtils::read_shader_file("Unlit.frag"), FileUtils::get_shader_path("Unlit.frag"));
    vk_context->context->create_named_pipeline("unlit_pipeline", pci);
  }

  wait_for_futures(vk_context);
}

static uint32_t get_material_index(const std::unordered_map<uint32_t, uint32_t>& material_map, uint32_t mesh_index, uint32_t material_index) {
  OX_SCOPED_ZONE;
  uint32_t size = 0;
  for (uint32_t i = 0; i < mesh_index; i++)
    size += material_map.at(i);
  return size + material_index;
}

static std::pair<vuk::Resource, vuk::Name> get_attachment_or_black(const char* name, const bool enabled, const vuk::Access access = vuk::eFragmentSampled) {
  if (enabled)
    return {vuk::Resource(name, vuk::Resource::Type::eImage, access), name};

  return {vuk::Resource("black_image", vuk::Resource::Type::eImage, access), "black_image"};
}

static std::pair<vuk::Resource, vuk::Name> get_attachment_or_black_uint(const char* name, const bool enabled, const vuk::Access access = vuk::eFragmentSampled) {
  if (enabled)
    return {vuk::Resource(name, vuk::Resource::Type::eImage, access), name};

  return {vuk::Resource("black_image_uint", vuk::Resource::Type::eImage, access), "black_image_uint"};
}

void DefaultRenderPipeline::debug_pass(const Ref<vuk::RenderGraph>& rg, const vuk::Name dst, const char* depth, vuk::Allocator& frame_allocator) const {
  struct DebugPassData {
    Mat4 vp = {};
    Mat4 model = {};
    Vec4 color = {};
  };

  const auto* camera = m_renderer_context.current_camera;
  std::vector<DebugPassData> buffers;

  auto& shapes = DebugRenderer::get_instance()->get_shapes();
  for (auto& shape : shapes) {
    DebugPassData data;
    data.vp = camera->get_projection_matrix_flipped() * camera->get_view_matrix();
    data.model = shape.model_matrix;
    data.color = Vec4(0, 1, 0, 1);

    buffers.emplace_back(data);
  }

  auto [buff, buff_fut] = create_buffer(frame_allocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(buffers));
  auto& buffer = *buff;

  rg->add_pass({
    .name = "debug_shapes_pass",
    .resources = {
      vuk::Resource(dst, vuk::Resource::Type::eImage, vuk::eColorWrite, dst.append("+")),
      vuk::Resource(depth, vuk::Resource::Type::eImage, vuk::eDepthStencilRead)
    },
    .execute = [this, buffer](vuk::CommandBuffer& command_buffer) {
      auto& shapes = DebugRenderer::get_instance()->get_shapes();

      command_buffer.bind_graphics_pipeline("unlit_pipeline")
                    .set_depth_stencil({
                       .depthTestEnable = true,
                       .depthWriteEnable = false,
                       .depthCompareOp = vuk::CompareOp::eLessOrEqual
                     })
                    .broadcast_color_blend({})
                    .set_rasterization({.cullMode = vuk::CullModeFlagBits::eFront})
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .bind_buffer(0, 0, buffer);

      for (uint32_t i = 0; i < (uint32_t)shapes.size(); i++) {
        const struct PushConst {
          uint32_t index;
        } pc = {i};
        command_buffer.push_constants(vuk::ShaderStageFlagBits::eVertex, 0, pc);
        shapes[i].shape_mesh->bind_vertex_buffer(command_buffer);
        shapes[i].shape_mesh->bind_index_buffer(command_buffer);
        for (auto node : shapes[i].shape_mesh->nodes) {
          Renderer::render_node(node, command_buffer, [](Mesh::Primitive*, Mesh::MeshData*) { return true; });
        }
      }

      DebugRenderer::reset(false);
    }
  });
}

Scope<vuk::Future> DefaultRenderPipeline::on_render(vuk::Allocator& frame_allocator, const vuk::Future& target, vuk::Dimension3D dim) {
  if (!m_renderer_context.current_camera) {
    OX_CORE_FATAL("No camera is set for rendering!");
    // set a temporary one
    if (!default_camera)
      default_camera = create_ref<Camera>();
    m_renderer_context.current_camera = default_camera.get();
  }

  auto vk_context = VulkanContext::get();

  if (m_should_merge_render_objects) {
    auto [index_buffer, vertex_buffer] = RendererCommon::merge_render_objects(mesh_draw_list);
    m_merged_index_buffer = index_buffer;
    m_merged_vertex_buffer = vertex_buffer;
    m_should_merge_render_objects = false;
  }

  const auto rg = create_ref<vuk::RenderGraph>("DefaultRenderPipelineRenderGraph");

  m_renderer_data.ubo_vs.view = m_renderer_context.current_camera->get_view_matrix();
  m_renderer_data.ubo_vs.cam_pos = m_renderer_context.current_camera->get_position();
  m_renderer_data.ubo_vs.projection = m_renderer_context.current_camera->get_projection_matrix_flipped();
  auto [vs_buff, vs_buffer_fut] = create_buffer(frame_allocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(&m_renderer_data.ubo_vs, 1));
  auto& vs_buffer = *vs_buff;

  rg->attach_and_clear_image("black_image", {.format = vk_context->swapchain->format, .sample_count = vuk::SampleCountFlagBits::e1}, vuk::Black<float>);
  rg->inference_rule("black_image", vuk::same_shape_as("final_image"));

  rg->attach_and_clear_image("black_image_uint", {.format = vuk::Format::eR8Uint, .sample_count = vuk::SampleCountFlagBits::e1}, vuk::Black<float>);
  rg->inference_rule("black_image_uint", vuk::same_shape_as("final_image"));


  std::vector<Material::Parameters> materials = {};

  std::unordered_map<uint32_t, uint32_t> material_map; //mesh, material

  for (uint32_t i = 0; i < static_cast<uint32_t>(mesh_draw_list.size()); i++) {
    for (auto& mat : mesh_draw_list[i].materials)
      materials.emplace_back(mat->parameters);
    material_map.emplace(i, static_cast<uint32_t>(mesh_draw_list[i].materials.size()));
  }

  auto [matBuff, matBufferFut] = create_buffer(frame_allocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(materials));
  auto& mat_buffer = *matBuff;

  depth_pre_pass(rg, vs_buffer, material_map, mat_buffer);

  rg->attach_and_clear_image("normal_image", {.format = vuk::Format::eR16G16B16A16Sfloat, .sample_count = vuk::SampleCountFlagBits::e1}, vuk::Black<float>);
  rg->attach_and_clear_image("depth_image", {.format = vuk::Format::eD32Sfloat, .sample_count = vuk::SampleCountFlagBits::e1}, vuk::DepthOne);
  rg->inference_rule("normal_image", vuk::same_shape_as("final_image"));
  rg->inference_rule("depth_image", vuk::same_shape_as("final_image"));

  if (RendererCVar::cvar_gtao_enable.get())
    gtao_pass(frame_allocator, rg);

  auto [buffer, buffer_fut] = create_buffer(frame_allocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(&direct_shadow_ub, 1));
  auto& shadow_buffer = *buffer;

  cascaded_shadow_pass(rg, shadow_buffer);

  // fill it if it's empty. Or we could allow vulkan to bind null buffers with VK_EXT_robustness2 nullDescriptor feature. 
  if (point_lights_data.empty())
    point_lights_data.emplace_back(LightingData{Vec4{0}, Vec4{0}, Vec4{0}, Vec4{}});

  auto [point_lights_buf, point_lights_buffer_fut] = create_buffer(frame_allocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(point_lights_data));
  auto& point_lights_buffer = *point_lights_buf;

  struct PBRPassParams {
    int num_lights = 0;
    IVec2 num_threads = {};
    IVec2 screen_dimensions = {};
    IVec2 num_thread_groups = {};
  } pbr_pass_params;

  pbr_pass_params.num_lights = (int)point_lights_data.size();
  pbr_pass_params.num_threads = (glm::ivec2(Renderer::get_viewport_width(), Renderer::get_viewport_height()) + PIXELS_PER_TILE - 1) / PIXELS_PER_TILE;
  pbr_pass_params.num_thread_groups = (pbr_pass_params.num_threads + TILES_PER_THREADGROUP - 1) / TILES_PER_THREADGROUP;
  pbr_pass_params.screen_dimensions = glm::ivec2(Renderer::get_viewport_width(), Renderer::get_viewport_height());

  point_lights_data.clear();
  spot_lights_data.clear();

  auto [pbr_buf, pbr_buffer_fut] = create_buffer(frame_allocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(&pbr_pass_params, 1));
  auto& pbr_buffer = *pbr_buf;

  geomerty_pass(rg, vs_buffer, material_map, mat_buffer, shadow_buffer, point_lights_buffer, pbr_buffer);

  rg->attach_and_clear_image("pbr_image", {.format = vk_context->swapchain->format, .sample_count = vuk::SampleCountFlagBits::e1}, vuk::Black<float>);
  rg->attach_and_clear_image("pbr_depth", {.format = vuk::Format::eD32Sfloat, .sample_count = vuk::SampleCountFlagBits::e1}, vuk::DepthOne);
  rg->inference_rule("pbr_image", vuk::same_shape_as("final_image"));
  rg->inference_rule("pbr_depth", vuk::same_shape_as("final_image"));

  if (RendererCVar::cvar_ssr_enable.get())
    ssr_pass(frame_allocator, rg, vk_context, vs_buffer);

  if (RendererCVar::cvar_bloom_enable.get())
    bloom_pass(rg, vk_context);

  m_renderer_data.final_pass_data.tonemapper = RendererCVar::cvar_tonemapper.get();
  m_renderer_data.final_pass_data.exposure = RendererCVar::cvar_exposure.get();
  m_renderer_data.final_pass_data.gamma = RendererCVar::cvar_gamma.get();
  m_renderer_data.final_pass_data.enable_bloom = RendererCVar::cvar_bloom_enable.get();
  m_renderer_data.final_pass_data.enable_ssr = RendererCVar::cvar_ssr_enable.get();
  m_renderer_data.final_pass_data.enable_ssao = RendererCVar::cvar_gtao_enable.get();

  auto [final_buff, final_buff_fut] = create_buffer(frame_allocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(&m_renderer_data.final_pass_data, 1));
  auto& final_buffer = *final_buff;

  auto [ssr_resouce, ssr_name] = get_attachment_or_black("ssr_output", RendererCVar::cvar_ssr_enable.get());
  auto [gtao_resouce, gtao_name] = get_attachment_or_black_uint("gtao_final_output", RendererCVar::cvar_gtao_enable.get());
  auto [bloom_resource, bloom_name] = get_attachment_or_black("bloom_upsampled_image", RendererCVar::cvar_bloom_enable.get());

  std::vector<vuk::Resource> final_resources = {
    "final_image"_image >> vuk::eColorRW >> "final_output",
    "pbr_output"_image >> vuk::eFragmentSampled,
    ssr_resouce,
    gtao_resouce,
    bloom_resource,
  };

  auto final_image_attachment = vuk::ImageAttachment{
    .extent = dim,
    .format = vk_context->swapchain->format,
    .sample_count = vuk::Samples::e1,
    .level_count = 1,
    .layer_count = 1
  };
  rg->attach_and_clear_image("final_image", final_image_attachment, vuk::Black<float>);

  rg->add_pass({
    .name = "final_pass",
    .resources = std::move(final_resources),
    .execute = [this, final_buffer, ssr_name, gtao_name, bloom_name](vuk::CommandBuffer& command_buffer) {
      this->mesh_draw_list.clear();

      command_buffer.bind_graphics_pipeline("final_pipeline")
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .broadcast_color_blend(vuk::BlendPreset::eOff)
                    .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
                    .bind_sampler(0, 0, vuk::LinearSamplerClamped)
                    .bind_image(0, 0, "pbr_output")
                    .bind_sampler(0, 3, vuk::LinearSamplerClamped)
                    .bind_image(0, 3, ssr_name)
                    .bind_sampler(0, 4, {})
                    .bind_image(0, 4, gtao_name)
                    .bind_sampler(0, 5, {})
                    .bind_image(0, 5, bloom_name)
                    .bind_buffer(0, 6, final_buffer)
                    .draw(3, 1, 0, 0);
    }
  });

  vuk::Name final_image_name = "final_output";

  if (RendererCVar::cvar_draw_grid.get()) {
    apply_grid(rg.get(), "final_output", "depth_output", frame_allocator);
    final_image_name = final_image_name.append("+");
  }

  if (RendererCVar::cvar_enable_debug_renderer.get()) {
    if (!DebugRenderer::get_instance()->get_shapes().empty()) {
      debug_pass(rg, final_image_name, "depth_output", frame_allocator);
      final_image_name = final_image_name.append("+");
    }
  }

  auto final_image_fut = vuk::Future{rg, final_image_name};

  if (RendererCVar::cvar_fxaa_enable.get()) {
    struct FXAAData {
      Vec2 inverse_screen_size;
    } fxaa_data;

    fxaa_data.inverse_screen_size = 1.0f / glm::vec2(Renderer::get_viewport_width(), Renderer::get_viewport_height());
    auto [fxaa_buff, fxaa_buffer_fut] = create_buffer(frame_allocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(&fxaa_data, 1));
    auto& fxaa_buffer = *fxaa_buff;
    final_image_fut = apply_fxaa(final_image_fut, target, fxaa_buffer);
  }

  return create_scope<vuk::Future>(std::move(final_image_fut));
}

void DefaultRenderPipeline::update_skybox(const SkyboxLoadEvent& e) {
  cube_map = e.cube_map;

  generate_prefilter();
}

void DefaultRenderPipeline::depth_pre_pass(const Ref<vuk::RenderGraph>& rg, vuk::Buffer& vs_buffer, const std::unordered_map<uint32_t, uint32_t>& material_map, vuk::Buffer& mat_buffer) {
  rg->add_pass({
    .name = "depth_pre_pass",
    .resources = {
      "normal_image"_image >> vuk::eColorRW >> "normal_output",
      "depth_image"_image >> vuk::eDepthStencilRW >> "depth_output"
    },
    .execute = [this, vs_buffer, mat_buffer, material_map](vuk::CommandBuffer& command_buffer) {
      // Skybox pass
      struct SkyboxPushConstant {
        Mat4 view;
        float lod_bias;
      } skybox_push_constant = {};

      skybox_push_constant.view = m_renderer_context.current_camera->skybox_view;

      command_buffer.bind_graphics_pipeline("skybox_pipeline")
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .broadcast_color_blend({})
                    .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
                    .set_depth_stencil(vuk::PipelineDepthStencilStateCreateInfo{
                       .depthTestEnable = false,
                       .depthWriteEnable = false,
                       .depthCompareOp = vuk::CompareOp::eLessOrEqual,
                     })
                    .bind_buffer(0, 0, vs_buffer)
                    .bind_sampler(0, 1, vuk::LinearSamplerRepeated)
                    .bind_image(0, 1, cube_map->as_attachment())
                    .push_constants(vuk::ShaderStageFlagBits::eVertex | vuk::ShaderStageFlagBits::eFragment, 0, skybox_push_constant);

      m_cube->bind_index_buffer(command_buffer);
      m_cube->bind_vertex_buffer(command_buffer);
      command_buffer.draw_indexed(m_cube->indices.size(), 1, 0, 0, 0);

      command_buffer.set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .broadcast_color_blend(vuk::BlendPreset::eOff)
                    .set_rasterization({.cullMode = vuk::CullModeFlagBits::eBack})
                    .set_depth_stencil(vuk::PipelineDepthStencilStateCreateInfo{
                       .depthTestEnable = true,
                       .depthWriteEnable = true,
                       .depthCompareOp = vuk::CompareOp::eLessOrEqual,
                     })
                    .bind_graphics_pipeline("depth_pre_pass_pipeline")
                    .bind_buffer(0, 0, vs_buffer)
                    .bind_buffer(2, 0, mat_buffer);

      for (uint32_t i = 0; i < mesh_draw_list.size(); i++) {
        auto& mesh = mesh_draw_list[i];

        Renderer::render_mesh(mesh,
          command_buffer,
          [&mesh, &command_buffer, i, material_map](const Mesh::Primitive* part, Mesh::MeshData* mesh_data) {
            const auto& material = mesh.materials[part->material_index];

            if (material->parameters.alpha_mode == (uint32_t)Material::AlphaMode::Mask) {
              return false;
            }

            command_buffer.bind_buffer(3, 0, *mesh_data->node_buffer)
                          .push_constants(vuk::ShaderStageFlagBits::eVertex | vuk::ShaderStageFlagBits::eFragment, 0, mesh.transform)
                          .push_constants(vuk::ShaderStageFlagBits::eFragment, sizeof(glm::mat4), get_material_index(material_map, i, part->material_index))
                          .bind_sampler(1, 0, vuk::LinearSamplerRepeated)
                          .bind_image(1, 0, *material->normal_texture->get_texture().view)
                          .bind_sampler(1, 1, vuk::LinearSamplerRepeated)
                          .bind_image(1, 1, *material->metallic_roughness_texture->get_texture().view);
            return true;
          });
      }
    }
  });
}

void DefaultRenderPipeline::geomerty_pass(const Ref<vuk::RenderGraph>& rg,
                                          vuk::Buffer& vs_buffer,
                                          const std::unordered_map<uint32_t, uint32_t>& material_map,
                                          vuk::Buffer& mat_buffer,
                                          vuk::Buffer& shadow_buffer,
                                          vuk::Buffer& point_lights_buffer,
                                          vuk::Buffer pbr_buffer) {
  rg->add_pass({
    .name = "geomerty_pass",
    .resources = {
      "pbr_image"_image >> vuk::eColorRW >> "pbr_output",
      "pbr_depth"_image >> vuk::eDepthStencilRW,
      "shadow_array_output"_image >> vuk::eFragmentSampled
    },
    .execute = [this, vs_buffer, point_lights_buffer, shadow_buffer, pbr_buffer, mat_buffer, material_map](vuk::CommandBuffer& command_buffer) {
      // Skybox pass
      struct SkyboxPushConstant {
        Mat4 view;
        float lod_bias;
      } skybox_push_constant = {};

      skybox_push_constant.view = m_renderer_context.current_camera->skybox_view;

      command_buffer.bind_graphics_pipeline("skybox_pipeline")
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .broadcast_color_blend({})
                    .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
                    .set_depth_stencil(vuk::PipelineDepthStencilStateCreateInfo{
                       .depthTestEnable = false,
                       .depthWriteEnable = false,
                       .depthCompareOp = vuk::CompareOp::eLessOrEqual,
                     })
                    .bind_buffer(0, 0, vs_buffer)
                    .bind_sampler(0, 1, vuk::LinearSamplerRepeated)
                    .bind_image(0, 1, cube_map->as_attachment())
                    .push_constants(vuk::ShaderStageFlagBits::eVertex | vuk::ShaderStageFlagBits::eFragment, 0, skybox_push_constant);

      m_cube->bind_index_buffer(command_buffer);
      m_cube->bind_vertex_buffer(command_buffer);
      command_buffer.draw_indexed(m_cube->indices.size(), 1, 0, 0, 0);

      auto irradiance_att = vuk::ImageAttachment{
        .image = *irradiance_image,
        .image_flags = vuk::ImageCreateFlagBits::eCubeCompatible,
        .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
        .extent = vuk::Dimension3D::absolute(64, 64, 1),
        .format = vuk::Format::eR32G32B32A32Sfloat,
        .sample_count = vuk::Samples::e1,
        .view_type = vuk::ImageViewType::eCube,
        .base_level = 0,
        .level_count = 1,
        .base_layer = 0,
        .layer_count = 6
      };

      auto prefilter_att = vuk::ImageAttachment{
        .image = *prefiltered_image,
        .image_flags = vuk::ImageCreateFlagBits::eCubeCompatible,
        .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
        .extent = vuk::Dimension3D::absolute(512, 512, 1),
        .format = vuk::Format::eR32G32B32A32Sfloat,
        .sample_count = vuk::Samples::e1,
        .view_type = vuk::ImageViewType::eCube,
        .base_level = 0,
        .level_count = 1,
        .base_layer = 0,
        .layer_count = 6
      };

      auto brdf_att = vuk::ImageAttachment{
        .image = *brdf_image,
        .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
        .extent = vuk::Dimension3D::absolute(512, 512, 1),
        .format = vuk::Format::eR32G32B32A32Sfloat,
        .sample_count = vuk::Samples::e1,
        .view_type = vuk::ImageViewType::e2D,
        .base_level = 0,
        .level_count = 1,
        .base_layer = 0,
        .layer_count = 1
      };

      command_buffer.bind_graphics_pipeline("pbr_pipeline")
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .broadcast_color_blend({})
                    .set_depth_stencil(vuk::PipelineDepthStencilStateCreateInfo{
                       .depthTestEnable = true,
                       .depthWriteEnable = true,
                       .depthCompareOp = vuk::CompareOp::eLessOrEqual,
                     })
                    .bind_buffer(0, 0, vs_buffer)
                    .bind_buffer(0, 1, pbr_buffer)
                    .bind_buffer(0, 2, point_lights_buffer)
                    .bind_sampler(0, 4, vuk::LinearSamplerRepeated)
                    .bind_image(0, 4, irradiance_att)
                    .bind_sampler(0, 5, vuk::LinearSamplerRepeated)
                    .bind_image(0, 5, brdf_att)
                    .bind_sampler(0, 6, vuk::LinearSamplerRepeated)
                    .bind_image(0, 6, prefilter_att)
                    .bind_sampler(0, 7, vuk::LinearSamplerRepeated)
                    .bind_image(0, 7, "shadow_array_output")
                    .bind_buffer(0, 8, shadow_buffer)
                    .bind_buffer(2, 0, mat_buffer);

      // PBR pipeline
      for (uint32_t i = 0; i < mesh_draw_list.size(); i++) {
        auto& mesh = mesh_draw_list[i];
        Renderer::render_mesh(mesh,
          command_buffer,
          [&](const Mesh::Primitive* part, Mesh::MeshData* mesh_data) {
            const auto& material = mesh.materials[part->material_index];
            if (material->parameters.alpha_mode == (uint32_t)Material::AlphaMode::Blend) {
              transparent_mesh_draw_list.emplace_back(i);
              return false;
            }

            command_buffer.bind_buffer(3, 0, *mesh_data->node_buffer)
                          .push_constants(vuk::ShaderStageFlagBits::eVertex | vuk::ShaderStageFlagBits::eFragment, 0, mesh.transform)
                          .push_constants(vuk::ShaderStageFlagBits::eFragment, sizeof(glm::mat4), get_material_index(material_map, i, part->material_index));

            material->bind_textures(command_buffer);
            return true;
          });
      }

      // Transparency pass
      command_buffer.bind_graphics_pipeline("pbr_pipeline");
      command_buffer.broadcast_color_blend(vuk::BlendPreset::eAlphaBlend);
      for (auto i : transparent_mesh_draw_list) {
        auto& mesh = mesh_draw_list[i];
        Renderer::render_mesh(mesh,
          command_buffer,
          [&](const Mesh::Primitive* part, Mesh::MeshData*) {
            const auto& material = mesh.materials[part->material_index];
            command_buffer.push_constants(vuk::ShaderStageFlagBits::eVertex | vuk::ShaderStageFlagBits::eFragment, 0, mesh.transform)
                          .push_constants(vuk::ShaderStageFlagBits::eFragment, sizeof(glm::mat4), get_material_index(material_map, i, part->material_index));

            material->bind_textures(command_buffer);
            return true;
          });
      }
      transparent_mesh_draw_list.clear();
    }
  });
}

void DefaultRenderPipeline::bloom_pass(const Ref<vuk::RenderGraph>& rg, const VulkanContext* vk_context) {
  struct BloomPushConst {
    // x: threshold, y: clamp, z: radius, w: unused
    Vec4 params = {};
  } bloom_push_const;

  bloom_push_const.params.x = RendererCVar::cvar_bloom_threshold.get();
  bloom_push_const.params.y = RendererCVar::cvar_bloom_clamp.get();

  constexpr uint32_t bloom_mip_count = 8;

  rg->attach_and_clear_image("bloom_image", {.format = vk_context->swapchain->format, .sample_count = vuk::SampleCountFlagBits::e1, .level_count = bloom_mip_count, .layer_count = 1}, vuk::Black<float>);
  rg->inference_rule("bloom_image", vuk::same_extent_as("final_image"));

  auto [down_input_names, down_output_names] = diverge_image_mips(rg, "bloom_image", bloom_mip_count);

  rg->add_pass({
    .name = "bloom_prefilter",
    .resources = {
      vuk::Resource(down_input_names[0], vuk::Resource::Type::eImage, vuk::eComputeRW, down_output_names[0]),
      "pbr_output"_image >> vuk::eComputeSampled
    },
    .execute = [bloom_push_const, down_input_names](vuk::CommandBuffer& command_buffer) {
      command_buffer.bind_compute_pipeline("bloom_prefilter_pipeline")
                    .push_constants(vuk::ShaderStageFlagBits::eCompute, 0, bloom_push_const)
                    .bind_image(0, 0, down_input_names[0])
                    .bind_sampler(0, 0, vuk::NearestMagLinearMinSamplerClamped)
                    .bind_image(0, 1, "pbr_output")
                    .bind_sampler(0, 1, vuk::NearestMagLinearMinSamplerClamped)
                    .dispatch((Renderer::get_viewport_width() + 8 - 1) / 8, (Renderer::get_viewport_height() + 8 - 1) / 8, 1);
    }
  });

  for (int32_t i = 1; i < (int32_t)bloom_mip_count; i++) {
    auto& input_name = down_output_names[i - 1];
    rg->add_pass({
      .name = "bloom_downsample",
      .resources = {
        vuk::Resource(down_input_names[i], vuk::Resource::Type::eImage, vuk::eComputeRW, down_output_names[i]),
        vuk::Resource(input_name, vuk::Resource::Type::eImage, vuk::eComputeSampled),
      },
      .execute = [down_input_names, input_name, i](vuk::CommandBuffer& command_buffer) {
        const auto size = IVec2(Renderer::get_viewport_width() / (1 << i), Renderer::get_viewport_height() / (1 << i));

        command_buffer.bind_compute_pipeline("bloom_downsample_pipeline")
                      .bind_image(0, 0, down_input_names[i])
                      .bind_sampler(0, 0, vuk::LinearMipmapNearestSamplerClamped)
                      .bind_image(0, 1, input_name)
                      .bind_sampler(0, 1, vuk::LinearMipmapNearestSamplerClamped)
                      .dispatch((size.x + 8 - 1) / 8, (size.y + 8 - 1) / 8, 1);
      }
    });
  }

  // Upsampling
  // https://www.froyok.fr/blog/2021-12-ue4-custom-bloom/resources/code/bloom_down_up_demo.jpg

  rg->attach_and_clear_image("bloom_upsample_image", {.format = vk_context->swapchain->format, .sample_count = vuk::SampleCountFlagBits::e1, .level_count = bloom_mip_count - 1, .layer_count = 1}, vuk::Black<float>);
  rg->inference_rule("bloom_upsample_image", vuk::same_extent_as("final_image"));

  auto [up_diverged_names, up_output_names] = diverge_image_mips(rg, "bloom_upsample_image", bloom_mip_count - 1);

  for (int32_t i = (int32_t)bloom_mip_count - 2; i >= 0; i--) {
    const auto input_name = i == (int32_t)bloom_mip_count - 2 ? down_output_names[i + 1] : up_output_names[i + 1];
    rg->add_pass({
      .name = "bloom_upsample",
      .resources = {
        vuk::Resource(up_diverged_names[i], vuk::Resource::Type::eImage, vuk::eComputeRW, up_output_names[i]),
        vuk::Resource(input_name, vuk::Resource::Type::eImage, vuk::eComputeSampled),
        vuk::Resource(down_output_names[i], vuk::Resource::Type::eImage, vuk::eComputeSampled),
        vuk::Resource(down_output_names.back(), vuk::Resource::Type::eImage, vuk::eComputeSampled)
      },
      .execute = [up_diverged_names, i, input_name, down_output_names](vuk::CommandBuffer& command_buffer) {
        const auto size = IVec2(Renderer::get_viewport_width() / (1 << i), Renderer::get_viewport_height() / (1 << i));

        command_buffer.bind_compute_pipeline("bloom_upsample_pipeline")
                      .bind_image(0, 0, up_diverged_names[i])
                      .bind_sampler(0, 0, vuk::NearestMagLinearMinSamplerClamped)
                      .bind_image(0, 1, input_name)
                      .bind_sampler(0, 1, vuk::NearestMagLinearMinSamplerClamped)
                      .bind_image(0, 2, down_output_names[i])
                      .bind_sampler(0, 2, vuk::NearestMagLinearMinSamplerClamped)
                      .dispatch((size.x + 8 - 1) / 8, (size.y + 8 - 1) / 8, 1);
      }
    });
  }

  rg->converge_image_explicit(up_output_names, "bloom_upsampled_image");
}

void DefaultRenderPipeline::gtao_pass(vuk::Allocator& frame_allocator, const Ref<vuk::RenderGraph>& rg) {
  gtao_settings.QualityLevel = RendererCVar::cvar_gtao_quality_level.get();
  gtao_settings.DenoisePasses = RendererCVar::cvar_gtao_denoise_passes.get();
  gtao_settings.Radius = RendererCVar::cvar_gtao_radius.get();
  gtao_settings.RadiusMultiplier = 1.0f;
  gtao_settings.FalloffRange = RendererCVar::cvar_gtao_falloff_range.get();
  gtao_settings.SampleDistributionPower = RendererCVar::cvar_gtao_sample_distribution_power.get();
  gtao_settings.ThinOccluderCompensation = RendererCVar::cvar_gtao_thin_occluder_compensation.get();
  gtao_settings.FinalValuePower = RendererCVar::cvar_gtao_final_value_power.get();
  gtao_settings.DepthMIPSamplingOffset = RendererCVar::cvar_gtao_depth_mip_sampling_offset.get();

  gtao_update_constants(gtao_constants, (int)Renderer::get_viewport_width(), (int)Renderer::get_viewport_height(), gtao_settings, m_renderer_context.current_camera, 0);

  auto [gtao_const_buff, gtao_const_buff_fut] = create_buffer(frame_allocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(&gtao_constants, 1));
  auto& gtao_const_buffer = *gtao_const_buff;

  rg->attach_and_clear_image("gtao_depth_image", {.format = vuk::Format::eR32Sfloat, .sample_count = vuk::SampleCountFlagBits::e1, .level_count = 5, .layer_count = 1}, vuk::Black<float>);
  rg->inference_rule("gtao_depth_image", vuk::same_extent_as("final_image"));
  auto [diverged_names, output_names] = diverge_image_mips(rg, "gtao_depth_image", 5);

  rg->add_pass({
    .name = "gtao_first_pass",
    .resources = {
      vuk::Resource(diverged_names[0], vuk::Resource::Type::eImage, vuk::eComputeRW, output_names[0]),
      vuk::Resource(diverged_names[1], vuk::Resource::Type::eImage, vuk::eComputeRW, output_names[1]),
      vuk::Resource(diverged_names[2], vuk::Resource::Type::eImage, vuk::eComputeRW, output_names[2]),
      vuk::Resource(diverged_names[3], vuk::Resource::Type::eImage, vuk::eComputeRW, output_names[3]),
      vuk::Resource(diverged_names[4], vuk::Resource::Type::eImage, vuk::eComputeRW, output_names[4]),
      "depth_output"_image >> vuk::eComputeSampled
    },
    .execute = [gtao_const_buffer, diverged_names](vuk::CommandBuffer& command_buffer) {
      command_buffer.bind_compute_pipeline("gtao_first_pipeline")
                    .bind_buffer(0, 0, gtao_const_buffer)
                    .bind_image(0, 1, "depth_output")
                    .bind_image(0, 2, diverged_names[0])
                    .bind_image(0, 3, diverged_names[1])
                    .bind_image(0, 4, diverged_names[2])
                    .bind_image(0, 5, diverged_names[3])
                    .bind_image(0, 6, diverged_names[4])
                    .bind_sampler(0, 7, vuk::LinearSamplerClamped)
                    .dispatch((Renderer::get_viewport_width() + 16 - 1) / 16, (Renderer::get_viewport_height() + 16 - 1) / 16);
    }
  });

  rg->converge_image_explicit(output_names, "gtao_depth_output");

  rg->add_pass({
    .name = "gtao_main_pass",
    .resources = {
      "gtao_main_image"_image >> vuk::eComputeRW >> "gtao_main_output",
      "gtao_edge_image"_image >> vuk::eComputeRW >> "gtao_edge_output",
      "gtao_depth_output"_image >> vuk::eComputeSampled,
      "normal_output"_image >> vuk::eComputeSampled
    },
    .execute = [gtao_const_buffer](vuk::CommandBuffer& command_buffer) {
      command_buffer.bind_compute_pipeline("gtao_main_pipeline")
                    .bind_buffer(0, 0, gtao_const_buffer)
                    .bind_image(0, 1, "gtao_depth_output")
                    .bind_image(0, 2, "normal_output")
                    .bind_image(0, 3, "gtao_main_image")
                    .bind_image(0, 4, "gtao_edge_image")
                    .bind_sampler(0, 5, vuk::LinearSamplerClamped)
                    .dispatch((Renderer::get_viewport_width() + 8 - 1) / 8, (Renderer::get_viewport_height() + 8 - 1) / 8);
    }
  });

  // if bent normals used -> R32Uint
  // else R8Uint

  rg->attach_and_clear_image("gtao_main_image", {.format = vuk::Format::eR8Uint, .sample_count = vuk::SampleCountFlagBits::e1, .view_type = vuk::ImageViewType::e2D, .level_count = 1, .layer_count = 1}, vuk::Black<float>);
  rg->attach_and_clear_image("gtao_edge_image", {.format = vuk::Format::eR8Unorm, .sample_count = vuk::SampleCountFlagBits::e1, .view_type = vuk::ImageViewType::e2D, .level_count = 1, .layer_count = 1}, vuk::Black<float>);
  rg->inference_rule("gtao_main_image", vuk::same_extent_as("final_image"));
  rg->inference_rule("gtao_edge_image", vuk::same_extent_as("final_image"));

  const int pass_count = std::max(1, gtao_settings.DenoisePasses); // should be at least one for now.
  auto gtao_denoise_output = vuk::Name(fmt::format("gtao_denoised_image_{}_output", pass_count - 1));
  for (int i = 0; i < pass_count; i++) {
    auto attachment_name = vuk::Name(fmt::format("gtao_denoised_image_{}", i));
    rg->attach_and_clear_image(attachment_name, {.format = vuk::Format::eR8Uint, .sample_count = vuk::SampleCountFlagBits::e1, .view_type = vuk::ImageViewType::e2D, .level_count = 1, .layer_count = 1}, vuk::Black<float>);
    rg->inference_rule(attachment_name, vuk::same_extent_as("final_image"));

    auto read_input = i == 0 ? vuk::Name("gtao_main_output") : vuk::Name(fmt::format("gtao_denoised_image_{}_output", i - 1));
    rg->add_pass({
      .name = "gtao_denoise_pass",
      .resources = {
        vuk::Resource(attachment_name, vuk::Resource::Type::eImage, vuk::eComputeRW, attachment_name.append("_output")),
        vuk::Resource(read_input, vuk::Resource::Type::eImage, vuk::eComputeSampled),
        "gtao_edge_output"_image >> vuk::eComputeSampled
      },
      .execute = [gtao_const_buffer, attachment_name, read_input](vuk::CommandBuffer& commandBuffer) {
        commandBuffer.bind_compute_pipeline("gtao_denoise_pipeline")
                     .bind_buffer(0, 0, gtao_const_buffer)
                     .bind_image(0, 1, read_input)
                     .bind_image(0, 2, "gtao_edge_output")
                     .bind_image(0, 3, attachment_name)
                     .bind_sampler(0, 4, {})
                     .dispatch((Renderer::get_viewport_width() + (XE_GTAO_NUMTHREADS_X * 2) - 1) / (XE_GTAO_NUMTHREADS_X * 2), (Renderer::get_viewport_height() + XE_GTAO_NUMTHREADS_Y - 1) / XE_GTAO_NUMTHREADS_Y, 1);
      }
    });
  }

  rg->add_pass({
    .name = "gtao_final_pass",
    .resources = {
      "gtao_final_image"_image >> vuk::eComputeRW >> "gtao_final_output",
      vuk::Resource(gtao_denoise_output, vuk::Resource::Type::eImage, vuk::eComputeSampled),
      "gtao_edge_output"_image >> vuk::eComputeSampled
    },
    .execute = [gtao_const_buffer, gtao_denoise_output](vuk::CommandBuffer& command_buffer) {
      command_buffer.bind_compute_pipeline("gtao_final_pipeline")
                    .bind_buffer(0, 0, gtao_const_buffer)
                    .bind_image(0, 1, gtao_denoise_output)
                    .bind_image(0, 2, "gtao_edge_output")
                    .bind_image(0, 3, "gtao_final_image")
                    .bind_sampler(0, 4, {})
                    .dispatch((Renderer::get_viewport_width() + (XE_GTAO_NUMTHREADS_X * 2) - 1) / (XE_GTAO_NUMTHREADS_X * 2), (Renderer::get_viewport_height() + XE_GTAO_NUMTHREADS_Y - 1) / XE_GTAO_NUMTHREADS_Y, 1);
    }
  });

  // if bent normals used -> R32Uint
  // else R8Uint

  rg->attach_and_clear_image("gtao_final_image", {.format = vuk::Format::eR8Uint, .sample_count = vuk::SampleCountFlagBits::e1, .view_type = vuk::ImageViewType::e2D, .level_count = 1, .layer_count = 1}, vuk::Black<float>);
  rg->inference_rule("gtao_final_image", vuk::same_extent_as("final_image"));
}

void DefaultRenderPipeline::ssr_pass(vuk::Allocator& frame_allocator, const Ref<vuk::RenderGraph>& rg, VulkanContext* vk_context, vuk::Buffer& vs_buffer) const {
  struct SSRData {
    int samples = 30;
    int binary_search_samples = 8;
    float max_dist = 50.0f;
  } ssr_data;

  auto [ssr_buff, ssr_buff_fut] = create_buffer(frame_allocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(&ssr_data, 1));
  auto ssr_buffer = *ssr_buff;

  rg->add_pass({
    .name = "ssr_pass",
    .resources = {
      "ssr_image"_image >> vuk::eComputeRW >> "ssr_output",
      "pbr_output"_image >> vuk::eComputeSampled,
      "depth_output"_image >> vuk::eComputeSampled,
      "normal_output"_image >> vuk::eComputeSampled
    },
    .execute = [this, ssr_buffer, vs_buffer](vuk::CommandBuffer& command_buffer) {
      command_buffer.bind_compute_pipeline("ssr_pipeline")
                    .bind_sampler(0, 0, vuk::LinearSamplerClamped)
                    .bind_image(0, 0, "ssr_image")
                    .bind_sampler(0, 1, vuk::LinearSamplerClamped)
                    .bind_image(0, 1, "pbr_output")
                    .bind_sampler(0, 2, vuk::LinearSamplerClamped)
                    .bind_image(0, 2, "depth_output")
                    .bind_sampler(0, 3, vuk::LinearSamplerClamped)
                    .bind_image(0, 3, cube_map->as_attachment())
                    .bind_sampler(0, 4, vuk::LinearSamplerClamped)
                    .bind_image(0, 4, "normal_output")
                    .bind_buffer(0, 5, vs_buffer)
                    .bind_buffer(0, 6, ssr_buffer)
                    .dispatch((Renderer::get_viewport_width() + 8 - 1) / 8, (Renderer::get_viewport_height() + 8 - 1) / 8, 1);
    }
  });

  rg->attach_and_clear_image("ssr_image", {.format = vk_context->swapchain->format, .sample_count = vuk::SampleCountFlagBits::e1}, vuk::Black<float>);
  rg->inference_rule("ssr_image", vuk::same_shape_as("final_image"));
}

vuk::Future DefaultRenderPipeline::apply_fxaa(vuk::Future source, vuk::Future dst, vuk::Buffer& fxaa_buffer) {
  std::unique_ptr<vuk::RenderGraph> rgp = std::make_unique<vuk::RenderGraph>("fxaa");
  rgp->attach_in("jagged", std::move(source));
  rgp->attach_in("smooth", std::move(dst));
  rgp->add_pass({
    .name = "fxaa",
    .resources = {"jagged"_image >> vuk::eFragmentSampled, "smooth"_image >> vuk::eColorWrite},
    .execute = [fxaa_buffer](vuk::CommandBuffer& command_buffer) {
      command_buffer.bind_graphics_pipeline("fxaa_pipeline")
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .broadcast_color_blend(vuk::BlendPreset::eOff)
                    .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
                    .bind_image(0, 0, "jagged")
                    .bind_sampler(0, 0, vuk::LinearSamplerClamped)
                    .bind_buffer(0, 1, fxaa_buffer)
                    .draw(3, 1, 0, 0);
    }
  });
  rgp->inference_rule("jagged", vuk::same_shape_as("smooth"));
  rgp->inference_rule("smooth", vuk::same_shape_as("jagged"));
  return {std::move(rgp), "smooth+"};
}

void DefaultRenderPipeline::apply_grid(vuk::RenderGraph* rg, const vuk::Name dst, const vuk::Name depth_image, vuk::Allocator& frame_allocator) const {
  struct GridVertexBuffer {
    Mat4 view;
    Mat4 proj;
  } grid_vertex_data;

  grid_vertex_data.view = m_renderer_context.current_camera->get_view_matrix();
  grid_vertex_data.proj = m_renderer_context.current_camera->get_projection_matrix_flipped();

  auto [vgrid_buff, vgrid_buff_fut] = create_buffer(frame_allocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(&grid_vertex_data, 1));
  auto& grid_vertex_buffer = *vgrid_buff;

  struct GridFragmentBuffer {
    Vec4 camera_pos;
    float near;
    float far;
    float max_distance;
  } grid_fragment_data;

  grid_fragment_data.camera_pos = Vec4(m_renderer_context.current_camera->get_position(), 0.0);
  grid_fragment_data.near = m_renderer_context.current_camera->get_near();
  grid_fragment_data.far = m_renderer_context.current_camera->get_far();
  grid_fragment_data.max_distance = 10000.0f;

  auto [fgrid_buff, fgrid_buff_fut] = create_buffer(frame_allocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(&grid_fragment_data, 1));
  auto& grid_fragment_buffer = *fgrid_buff;
  rg->add_pass({
    .name = "grid",
    .resources = {
      vuk::Resource(dst, vuk::Resource::Type::eImage, vuk::eColorWrite, dst.append("+")),
      vuk::Resource(depth_image, vuk::Resource::Type::eImage, vuk::eDepthStencilRead)
    },
    .execute = [this, grid_vertex_buffer, grid_fragment_buffer](vuk::CommandBuffer& command_buffer) {
      command_buffer.bind_graphics_pipeline("grid_pipeline")
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .broadcast_color_blend(vuk::BlendPreset::eAlphaBlend)
                    .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
                    .set_depth_stencil({
                       .depthTestEnable = true,
                       .depthWriteEnable = false,
                       .depthCompareOp = vuk::CompareOp::eLessOrEqual
                     })
                    .bind_buffer(0, 0, grid_vertex_buffer)
                    .bind_buffer(0, 1, grid_fragment_buffer);

      m_quad->bind_index_buffer(command_buffer);
      m_quad->bind_vertex_buffer(command_buffer);
      command_buffer.draw_indexed(m_quad->indices.size(), 1, 0, 0, 0);
    }
  });
}

void DefaultRenderPipeline::cascaded_shadow_pass(const Ref<vuk::RenderGraph>& rg, vuk::Buffer& shadow_buffer) {
  Ref<vuk::RenderGraph> shadow_map = create_ref<vuk::RenderGraph>("shadow_map");
  const auto shadow_size = RendererCVar::cvar_shadows_size.get();
  const vuk::ImageAttachment shadow_array_attachment = {
    .extent = vuk::Dimension3D::absolute(shadow_size, shadow_size),
    .format = vuk::Format::eD16Unorm,
    .sample_count = vuk::SampleCountFlagBits::e1,
    .view_type = vuk::ImageViewType::e2DArray,
    .level_count = 1,
    .layer_count = 4
  };
  shadow_map->attach_and_clear_image("shadow_map_array", shadow_array_attachment, vuk::DepthOne);

  auto [diverged_cascade_names, _] = diverge_image_layers(shadow_map, "shadow_map_array", 4);

  std::vector<vuk::Name> final_diverged_cascade_names = {};

  for (uint32_t cascade_index = 0; cascade_index < SHADOW_MAP_CASCADE_COUNT; cascade_index++) {
    Ref<vuk::RenderGraph> shadow_map_face = create_ref<vuk::RenderGraph>("shadow_map_face");
    shadow_map_face->attach_in("shadow_map_face", {shadow_map, diverged_cascade_names[cascade_index]});

    shadow_map_face->add_pass({
      .name = "direct_shadow_pass",
      .resources = {
        "shadow_map_face"_image >> vuk::eDepthStencilRW >> "shadow_map_face_output"
      },
      .execute = [this, shadow_buffer, cascade_index](vuk::CommandBuffer& command_buffer) {
        command_buffer.bind_graphics_pipeline("shadow_pipeline")
                      .broadcast_color_blend({})
                      .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
                      .set_depth_stencil(vuk::PipelineDepthStencilStateCreateInfo{
                         .depthTestEnable = true,
                         .depthWriteEnable = true,
                         .depthCompareOp = vuk::CompareOp::eLessOrEqual,
                       })
                      .set_viewport(0, vuk::Rect2D::framebuffer())
                      .set_scissor(0, vuk::Rect2D::framebuffer())
                      .bind_buffer(0, 0, shadow_buffer);

        for (const auto& light : dir_lights_data) {
          DirectShadowPass::update_cascades(Vec3(light.rotation), m_renderer_context.current_camera, &direct_shadow_ub);

          for (uint32_t i = 0; i < mesh_draw_list.size(); i++) {
            auto& mesh = mesh_draw_list[i];
            struct PushConst {
              glm::mat4 model_matrix{};
              uint32_t cascade_index = 0;
            } push_const;
            push_const.model_matrix = mesh.transform;
            push_const.cascade_index = cascade_index;

            command_buffer.push_constants(vuk::ShaderStageFlagBits::eVertex, 0, push_const);
            Renderer::render_mesh(mesh, command_buffer, [](const Mesh::Primitive*, Mesh::MeshData*) { return true; });
          }
        }

        this->dir_lights_data.clear();
      }
    });
    vuk::Future face_fut = {std::move(shadow_map_face), "shadow_map_face_output"};
    auto layer_name = vuk::Name("shadow_map_face").append(std::to_string(cascade_index));
    final_diverged_cascade_names.emplace_back(layer_name);
    rg->attach_in(layer_name, std::move(face_fut));
  }

  rg->converge_image_explicit(final_diverged_cascade_names, "shadow_array_output");
}

void DefaultRenderPipeline::generate_prefilter() {
  OX_SCOPED_ZONE;
  vuk::Compiler compiler{};
  auto& allocator = *VulkanContext::get()->superframe_allocator;

  // blur cubemap layers
  const Ref<vuk::RenderGraph> rg = create_ref<vuk::RenderGraph>("cubemap_blurring");

  rg->attach_image("cubemap", vuk::ImageAttachment::from_texture(cube_map->get_texture()));
  auto [diverged_image, diverged_image_output] = vuk::diverge_image_layers(rg, "cubemap_image", 6);

  for (uint32_t i = 0; i < 6; i++) {
    RendererCommon::apply_blur(rg, "", diverged_image[i], diverged_image_output[i]);
  }

  auto [brdf_img, brdf_fut] = Prefilter::generate_brdflut();
  brdf_fut.wait(allocator, compiler);
  brdf_image = std::move(brdf_img);

  auto [irradiance_img, irradiance_fut] = Prefilter::generate_irradiance_cube(m_cube, cube_map);
  irradiance_fut.wait(allocator, compiler);
  irradiance_image = std::move(irradiance_img);

  auto [prefilter_img, prefilter_fut] = Prefilter::generate_prefiltered_cube(m_cube, cube_map);
  prefilter_fut.wait(allocator, compiler);
  prefiltered_image = std::move(prefilter_img);
}

void DefaultRenderPipeline::update_parameters(ProbeChangeEvent& e) {
  auto& ubo = m_renderer_data.final_pass_data;
  auto& component = e.probe;
  ubo.film_grain = {component.film_grain_enabled, component.film_grain_intensity};
  ubo.chromatic_aberration = {component.chromatic_aberration_enabled, component.chromatic_aberration_intensity};
  ubo.film_grain = {component.film_grain_enabled, component.film_grain_intensity};
  ubo.vignette_offset.w = component.vignette_enabled;
  ubo.vignette_color.a = component.vignette_intensity;
}
}
