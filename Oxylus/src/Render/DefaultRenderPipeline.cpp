#include "DefaultRenderPipeline.h"

#include <glm/gtc/type_ptr.inl>
#include <vuk/Partials.hpp>

#include "DebugRenderer.h"
#include "Window.h"
#include "Assets/AssetManager.h"
#include "Core/Entity.h"
#include "Core/Resources.h"
#include "PBR/DirectShadowPass.h"
#include "PBR/Prefilter.h"
#include "Utils/FileUtils.h"
#include "Utils/Profiler.h"
#include "Vulkan/VukUtils.h"
#include "Vulkan/VulkanContext.h"

#include "Vulkan/VulkanRenderer.h"

namespace Oxylus {
void DefaultRenderPipeline::init(Scene* scene) {
  m_scene = scene;
  // Lights data
  point_lights_data.reserve(MAX_NUM_LIGHTS);

  // Mesh data
  mesh_draw_list.reserve(MAX_NUM_MESHES);
  transparent_mesh_draw_list.reserve(MAX_NUM_MESHES);

  skybox_cube = create_ref<Mesh>();
  skybox_cube = AssetManager::get_mesh_asset(Resources::get_resources_path("Objects/cube.glb"));

  m_resources.cube_map = create_ref<TextureAsset>();
  m_resources.cube_map = AssetManager::get_texture_asset({.Path = Resources::get_resources_path("HDRs/table_mountain_2_puresky_2k.hdr"), .Format = vuk::Format::eR8G8B8A8Srgb});
  generate_prefilter();

  RendererConfig::get()->config_change_dispatcher.sink<RendererConfig::ConfigChangeEvent>().connect<&DefaultRenderPipeline::update_final_pass_data>(*this);

  init_render_graph();

  RendererConfig::get()->dispatch_config_change();
}

void DefaultRenderPipeline::update(Scene* scene) {
  // TODO:(hatrickek) Temporary solution for camera.
  m_renderer_context.current_camera = VulkanRenderer::renderer_context.current_camera;

  if (!m_renderer_context.current_camera)
    OX_CORE_FATAL("No camera is set for rendering!");

  // Mesh
  {
    OX_SCOPED_ZONE_N("Mesh System");
    const auto view = scene->m_registry.view<TransformComponent, MeshRendererComponent, MaterialComponent, TagComponent>();
    for (const auto&& [entity, transform, meshrenderer, material, tag] : view.each()) {
      auto e = Entity(entity, scene);
      auto parent = e.get_parent();
      bool parentEnabled = true;
      if (parent)
        parentEnabled = parent.get_component<TagComponent>().enabled;
      if (tag.enabled && parentEnabled && !material.materials.empty())
        mesh_draw_list.emplace_back(meshrenderer.mesh_geometry.get(), e.get_world_transform(), material.materials, meshrenderer.submesh_index);
    }
  }

  // Particle system
  {
    OX_SCOPED_ZONE_N("Particle System");
    const auto particleSystemView = scene->m_registry.view<TransformComponent, ParticleSystemComponent>();
    for (auto&& [e, tc, psc] : particleSystemView.each()) {
      psc.system->on_update(Application::get_timestep(), tc.translation);
      psc.system->on_render();
    }
  }

  // Lighting
  {
    OX_SCOPED_ZONE_N("Lighting System");
    // Scene lights
    {
      const auto view = scene->m_registry.view<TransformComponent, LightComponent>();
      for (auto&& [e,tc, lc] : view.each()) {
        Entity entity = {e, scene};
        if (!entity.get_component<TagComponent>().enabled)
          continue;
        switch (lc.type) {
          case LightComponent::LightType::Directional: {
            dir_lights_data.emplace_back(VulkanRenderer::LightingData{
              Vec4{tc.translation, lc.intensity},
              Vec4{lc.color, lc.range},
              Vec4{tc.rotation, 1.0f}
            });
            break;
          }
          case LightComponent::LightType::Point: {
            point_lights_data.emplace_back(VulkanRenderer::LightingData{
              Vec4{tc.translation, lc.intensity},
              Vec4{lc.color, lc.range},
              Vec4{tc.rotation, 1.0f}
            });
            break;
          }
          case LightComponent::LightType::Spot: {
            spot_lights_data.emplace_back(VulkanRenderer::LightingData{
              Vec4{tc.translation, lc.intensity},
              Vec4{lc.color, lc.range},
              Vec4{tc.rotation, 1.0f}
            });
            break;
          }
        }
      }
      light_buffer_dispatcher.trigger(LightChangeEvent{});
    }
  }
}

void DefaultRenderPipeline::on_dispatcher_events(EventDispatcher& dispatcher) {
  dispatcher.sink<SceneRenderer::SkyboxLoadEvent>().connect<&DefaultRenderPipeline::update_skybox>(*this);
  dispatcher.sink<SceneRenderer::ProbeChangeEvent>().connect<&DefaultRenderPipeline::update_parameters>(*this);
}

void DefaultRenderPipeline::shutdown() { }

void DefaultRenderPipeline::init_render_graph() {
  const auto vk_context = VulkanContext::get();

  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileUtils::read_shader_file("DepthNormalPass.vert"),
      FileUtils::get_shader_path("DepthNormalPass.vert"));
    pci.add_glsl(FileUtils::read_shader_file("DepthNormalPass.frag"),
      FileUtils::get_shader_path("DepthNormalPass.frag"));
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
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileUtils::read_shader_file("Unlit.vert"), "Unlit.vert");
    pci.add_glsl(FileUtils::read_shader_file("Unlit.frag"), "Unlit.frag");
    vk_context->context->create_named_pipeline("debug_pipeline", pci);
  }
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileUtils::read_shader_file("SSR.comp"), "SSR.comp");
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
    vk_context->context->create_named_pipeline("gtao_first_pipeline", pci);
  }
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_hlsl(FileUtils::read_shader_file("GTAO/GTAO_Main.hlsl"), FileUtils::get_shader_path("GTAO/GTAO_Main.hlsl"), vuk::HlslShaderStage::eCompute, "CSGTAOHigh");
    pci.define("XE_GTAO_FP32_DEPTHS", "");
    pci.define("XE_GTAO_USE_HALF_FLOAT_PRECISION", "0");
    pci.define("XE_GTAO_USE_DEFAULT_CONSTANTS", "0");
    vk_context->context->create_named_pipeline("gtao_main_pipeline", pci);
  }
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_hlsl(FileUtils::read_shader_file("GTAO/GTAO_Final.hlsl"), FileUtils::get_shader_path("GTAO/GTAO_Final.hlsl"), vuk::HlslShaderStage::eCompute, "CSDenoisePass");
    pci.define("XE_GTAO_FP32_DEPTHS", "");
    pci.define("XE_GTAO_USE_HALF_FLOAT_PRECISION", "0");
    pci.define("XE_GTAO_USE_DEFAULT_CONSTANTS", "0");
    vk_context->context->create_named_pipeline("gtao_denoise_pipeline", pci);
  }
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_hlsl(FileUtils::read_shader_file("GTAO/GTAO_Final.hlsl"), FileUtils::get_shader_path("GTAO/GTAO_Final.hlsl"), vuk::HlslShaderStage::eCompute, "CSDenoiseLastPass");
    pci.define("XE_GTAO_FP32_DEPTHS", "");
    pci.define("XE_GTAO_USE_HALF_FLOAT_PRECISION", "0");
    pci.define("XE_GTAO_USE_DEFAULT_CONSTANTS", "0");
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
    pci.add_glsl(FileUtils::read_shader_file("Bloom.comp"), FileUtils::get_shader_path("Bloom.comp"));
    vk_context->context->create_named_pipeline("bloom_pipeline", pci);
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

  wait_for_futures(vk_context);
}

static uint32_t get_material_index(const std::unordered_map<uint32_t, uint32_t>& material_map, uint32_t mesh_index, uint32_t material_index) {
  uint32_t size = 0;
  for (uint32_t i = 0; i < mesh_index; i++)
    size += material_map.at(i);
  return size + material_index;
}

Scope<vuk::Future> DefaultRenderPipeline::on_render(vuk::Allocator& frame_allocator, const vuk::Future& target, vuk::Dimension3D dim) {
  mesh_draw_list.clear();

  update(m_scene);

  const auto rg = create_ref<vuk::RenderGraph>("DefaultRenderPipelineRenderGraph");

  auto vk_context = VulkanContext::get();

  m_renderer_data.ubo_vs.view = m_renderer_context.current_camera->GetViewMatrix();
  m_renderer_data.ubo_vs.cam_pos = m_renderer_context.current_camera->GetPosition();
  m_renderer_data.ubo_vs.projection = m_renderer_context.current_camera->GetProjectionMatrixFlipped();
  auto [vs_buff, vs_buffer_fut] = create_buffer(frame_allocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(&m_renderer_data.ubo_vs, 1));
  auto& vs_buffer = *vs_buff;

  std::vector<Material::Parameters> materials = {};

  std::unordered_map<uint32_t, uint32_t> material_map; //mesh, material

  for (uint32_t i = 0; i < static_cast<uint32_t>(mesh_draw_list.size()); i++) {
    for (auto& mat : mesh_draw_list[i].materials)
      materials.emplace_back(mat->parameters);
    material_map.emplace(i, static_cast<uint32_t>(mesh_draw_list[i].materials.size()));
  }

  auto [matBuff, matBufferFut] = create_buffer(frame_allocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(materials));
  auto& mat_buffer = *matBuff;

  rg->add_pass({
    .name = "DepthPrePass",
    .resources = {
      "normal_image"_image >> vuk::eColorRW >> "normal_output",
      "depth_image"_image >> vuk::eDepthStencilRW >> "depth_output"
    },
    .execute = [this, vs_buffer, mat_buffer, material_map](vuk::CommandBuffer& commandBuffer) {
      commandBuffer.set_viewport(0, vuk::Rect2D::framebuffer())
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

        VulkanRenderer::render_mesh(mesh,
          commandBuffer,
          [&mesh, &commandBuffer, i, material_map](const Mesh::Primitive* part) {
            const auto& material = mesh.materials[part->material_index];

            commandBuffer.push_constants(vuk::ShaderStageFlagBits::eVertex | vuk::ShaderStageFlagBits::eFragment, 0, mesh.transform)
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

  rg->attach_and_clear_image("normal_image", {.format = vuk::Format::eR16G16B16A16Sfloat, .sample_count = vuk::SampleCountFlagBits::e1}, vuk::Black<float>);
  rg->attach_and_clear_image("depth_image", {.format = vuk::Format::eD32Sfloat, .sample_count = vuk::SampleCountFlagBits::e1}, vuk::DepthOne);
  rg->inference_rule("normal_image", vuk::same_shape_as("final_image"));
  rg->inference_rule("depth_image", vuk::same_shape_as("final_image"));

#pragma region GTAO

  gtao_update_constants(gtao_constants, (int)VulkanRenderer::get_viewport_width(), (int)VulkanRenderer::get_viewport_height(), gtao_settings, value_ptr(m_renderer_data.ubo_vs.projection), true, 0);

  auto [gtao_const_buff, gtao_const_buff_fut] = create_buffer(frame_allocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(&gtao_constants, 1));
  auto& gtao_const_buffer = *gtao_const_buff;

  rg->attach_and_clear_image("gtao_depth_image", {.format = vuk::Format::eR32Sfloat, .sample_count = vuk::SampleCountFlagBits::e1, .level_count = 5, .layer_count = 1}, vuk::Black<float>);
  rg->inference_rule("gtao_depth_image", vuk::same_extent_as("final_image"));

  auto diverged_names = diverge_image_mips(rg, "gtao_depth_image", 5);
  std::vector<vuk::Name> output_names = {};
  output_names.reserve(diverged_names.size());
  for (auto& n : diverged_names)
    output_names.emplace_back(n.append("+"));

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
                    .dispatch((VulkanRenderer::get_viewport_width() + 16 - 1) / 16, (VulkanRenderer::get_viewport_height() + 16 - 1) / 16);
    }
  });

  rg->converge_image_explicit(output_names, "gtao_depth_output");

  //converge_image_mips(rg, diverged_names, "gtao_depth_output");

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
                    .dispatch((VulkanRenderer::get_viewport_width() + 8 - 1) / 8, (VulkanRenderer::get_viewport_height() + 8 - 1) / 8);
    }
  });

  // if bent normals used -> R32Uint
  // else R8Uint

  rg->attach_and_clear_image("gtao_main_image", {.format = vuk::Format::eR8Uint, .sample_count = vuk::SampleCountFlagBits::e1, .view_type = vuk::ImageViewType::e2D, .level_count = 1, .layer_count = 1}, vuk::Black<float>);
  rg->attach_and_clear_image("gtao_edge_image", {.format = vuk::Format::eR8Unorm, .sample_count = vuk::SampleCountFlagBits::e1, .view_type = vuk::ImageViewType::e2D, .level_count = 1, .layer_count = 1}, vuk::Black<float>);
  rg->inference_rule("gtao_main_image", vuk::same_extent_as("final_image"));
  rg->inference_rule("gtao_edge_image", vuk::same_extent_as("final_image"));

  gtao_settings.DenoisePasses = 1;
  int pass_count = std::max(1, gtao_settings.DenoisePasses); // should be at least one for now.
  for (int i = 0; i < pass_count; i++) {
    rg->add_pass({
      .name = "gtao_denoise_pass",
      .resources = {
        "gtao_denoised_image"_image >> vuk::eComputeRW >> "gtao_denoised_output",
        "gtao_main_output"_image >> vuk::eComputeSampled,
        "gtao_edge_output"_image >> vuk::eComputeSampled
      },
      .execute = [gtao_const_buffer](vuk::CommandBuffer& commandBuffer) {
        commandBuffer.bind_compute_pipeline("gtao_denoise_pipeline")
                     .bind_buffer(0, 0, gtao_const_buffer)
                     .bind_image(0, 1, "gtao_main_output")
                     .bind_image(0, 2, "gtao_edge_output")
                     .bind_image(0, 3, "gtao_denoised_image")
                     .bind_sampler(0, 4, {})
                     .dispatch((VulkanRenderer::get_viewport_width() + (XE_GTAO_NUMTHREADS_X * 2) - 1) / (XE_GTAO_NUMTHREADS_X * 2), (VulkanRenderer::get_viewport_height() + XE_GTAO_NUMTHREADS_Y - 1) / XE_GTAO_NUMTHREADS_Y, 1);
      }
    });
  }

  rg->attach_and_clear_image("gtao_denoised_image", {.format = vuk::Format::eR8Uint, .sample_count = vuk::SampleCountFlagBits::e1, .view_type = vuk::ImageViewType::e2D, .level_count = 1, .layer_count = 1}, vuk::Black<float>);
  rg->inference_rule("gtao_denoised_image", vuk::same_extent_as("final_image"));

  rg->add_pass({
    .name = "gtao_final_pass",
    .resources = {
      "gtao_final_image"_image >> vuk::eComputeRW >> "gtao_final_output",
      "gtao_denoised_output"_image >> vuk::eComputeSampled,
      "gtao_edge_output"_image >> vuk::eComputeSampled
    },
    .execute = [gtao_const_buffer](vuk::CommandBuffer& commandBuffer) {
      commandBuffer.bind_compute_pipeline("gtao_final_pipeline")
                   .bind_buffer(0, 0, gtao_const_buffer)
                   .bind_image(0, 1, "gtao_denoised_output")
                   .bind_image(0, 2, "gtao_edge_output")
                   .bind_image(0, 3, "gtao_final_image")
                   .bind_sampler(0, 4, {})
                   .dispatch((VulkanRenderer::get_viewport_width() + (XE_GTAO_NUMTHREADS_X * 2) - 1) / (XE_GTAO_NUMTHREADS_X * 2), (VulkanRenderer::get_viewport_height() + XE_GTAO_NUMTHREADS_Y - 1) / XE_GTAO_NUMTHREADS_Y, 1);
    }
  });

  // if bent normals used -> R32Uint
  // else R8Uint

  rg->attach_and_clear_image("gtao_final_image", {.format = vuk::Format::eR8Uint, .sample_count = vuk::SampleCountFlagBits::e1, .view_type = vuk::ImageViewType::e2D, .level_count = 1, .layer_count = 1}, vuk::Black<float>);
  rg->inference_rule("gtao_final_image", vuk::same_extent_as("final_image"));

#pragma endregion

  auto [buffer, buffer_fut] = create_buffer(frame_allocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(&direct_shadow_ub, 1));
  auto& shadow_buffer = *buffer;

  cascaded_shadow_pass(rg, shadow_buffer);

  if (point_lights_data.empty())
    point_lights_data.emplace_back();

  auto [point_lights_buf, point_lights_buffer_fut] = create_buffer(frame_allocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(point_lights_data));
  auto& point_lights_buffer = *point_lights_buf;

  struct PBRPassParams {
    int num_lights = 0;
    IVec2 num_threads = {};
    IVec2 screen_dimensions = {};
    IVec2 num_thread_groups = {};
  } pbr_pass_params;

  pbr_pass_params.num_lights = 1;
  pbr_pass_params.num_threads = (glm::ivec2(VulkanRenderer::get_viewport_width(), VulkanRenderer::get_viewport_height()) + PIXELS_PER_TILE - 1) / PIXELS_PER_TILE;
  pbr_pass_params.num_thread_groups = (pbr_pass_params.num_threads + TILES_PER_THREADGROUP - 1) / TILES_PER_THREADGROUP;
  pbr_pass_params.screen_dimensions = glm::ivec2(VulkanRenderer::get_viewport_width(), VulkanRenderer::get_viewport_height());

  auto [pbr_buf, pbr_buffer_fut] = create_buffer(frame_allocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(&pbr_pass_params, 1));
  auto pbr_buffer = *pbr_buf;

  rg->add_pass({
    .name = "geomerty_pass",
    .resources = {
      "pbr_image"_image >> vuk::eColorRW >> "pbr_output",
      "pbr_depth"_image >> vuk::eDepthStencilRW,
      "shadow_array_output"_image >> vuk::eFragmentSampled
    },
    .execute = [this, vs_buffer, point_lights_buffer, shadow_buffer, pbr_buffer, mat_buffer, material_map](vuk::CommandBuffer& commandBuffer) {
      // Skybox pass
      struct SkyboxPushConstant {
        Mat4 view;
        float lod_bias;
      } skybox_push_constant = {};

      skybox_push_constant.view = m_renderer_context.current_camera->SkyboxView;

      const auto sky_light_view = m_scene->m_registry.view<SkyLightComponent>();
      for (auto&& [e, sc] : sky_light_view.each())
        skybox_push_constant.lod_bias = sc.lod_bias;

      commandBuffer.bind_graphics_pipeline("skybox_pipeline")
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
                   .bind_image(0, 1, m_resources.cube_map->as_attachment())
                   .push_constants(vuk::ShaderStageFlagBits::eVertex | vuk::ShaderStageFlagBits::eFragment, 0, skybox_push_constant);

      skybox_cube->draw(commandBuffer);

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

      commandBuffer.bind_graphics_pipeline("pbr_pipeline")
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
        VulkanRenderer::render_mesh(mesh,
          commandBuffer,
          [&](const Mesh::Primitive* part) {
            const auto& material = mesh.materials[part->material_index];
            if (material->alpha_mode == Material::AlphaMode::Blend) {
              transparent_mesh_draw_list.emplace_back(i);
              return false;
            }

            commandBuffer.push_constants(vuk::ShaderStageFlagBits::eVertex | vuk::ShaderStageFlagBits::eFragment, 0, mesh.transform)
                         .push_constants(vuk::ShaderStageFlagBits::eFragment, sizeof(glm::mat4), get_material_index(material_map, i, part->material_index));

            material->bind_textures(commandBuffer);
            return true;
          });
      }

      commandBuffer.broadcast_color_blend(vuk::BlendPreset::eAlphaBlend);

      // Transparency pass
      for (auto i : transparent_mesh_draw_list) {
        auto& mesh = mesh_draw_list[i];
        VulkanRenderer::render_mesh(mesh,
          commandBuffer,
          [&](const Mesh::Primitive* part) {
            const auto& material = mesh.materials[part->material_index];
            commandBuffer.push_constants(vuk::ShaderStageFlagBits::eVertex | vuk::ShaderStageFlagBits::eFragment, 0, mesh.transform)
                         .push_constants(vuk::ShaderStageFlagBits::eFragment, sizeof(glm::mat4), get_material_index(material_map, i, part->material_index));

            material->bind_textures(commandBuffer);
            return true;
          });
      }
      transparent_mesh_draw_list.clear();

      // Depth-tested debug renderer pass
#if 0
      auto& shapes = DebugRenderer::GetInstance()->GetShapes();
      
      struct DebugPassData {
        Mat4 ViewProjection = {};
        Mat4 Model = {};
        Vec4 Color = {};
      } pushConst;
      
      commandBuffer.bind_graphics_pipeline("DebugPipeline")
                   .set_viewport(0, vuk::Rect2D::framebuffer())
                   .set_scissor(0, vuk::Rect2D::framebuffer());
      
      pushConst.ViewProjection = m_RendererContext.CurrentCamera->GetProjectionMatrixFlipped() * m_RendererContext.CurrentCamera->GetViewMatrix();
      for (auto& shape : shapes) {
        pushConst.Model = shape.ModelMatrix;
        pushConst.Color = shape.Color;
        commandBuffer.push_constants(vuk::ShaderStageFlagBits::eVertex, 0, &pushConst);
        shape.ShapeMesh->Draw(commandBuffer);
      }
      
      DebugRenderer::Reset(false);
#endif
    }
  });

  rg->attach_and_clear_image("pbr_image", {.format = vk_context->swapchain->format, .sample_count = vuk::SampleCountFlagBits::e1}, vuk::Black<float>);
  rg->attach_and_clear_image("pbr_depth", {.format = vuk::Format::eD32Sfloat, .sample_count = vuk::SampleCountFlagBits::e1}, vuk::DepthOne);
  rg->inference_rule("pbr_image", vuk::same_shape_as("final_image"));
  rg->inference_rule("pbr_depth", vuk::same_shape_as("final_image"));

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
                    .bind_image(0, 3, m_resources.cube_map->as_attachment())
                    .bind_sampler(0, 4, vuk::LinearSamplerClamped)
                    .bind_image(0, 4, "normal_output")
                    .bind_buffer(0, 5, vs_buffer)
                    .bind_buffer(0, 6, ssr_buffer)
                    .dispatch((VulkanRenderer::get_viewport_width() + 8 - 1) / 8, (VulkanRenderer::get_viewport_height() + 8 - 1) / 8, 1);
    }
  });

  rg->attach_and_clear_image("ssr_image", {.format = vk_context->swapchain->format, .sample_count = vuk::SampleCountFlagBits::e1}, vuk::Black<float>);
  rg->inference_rule("ssr_image", vuk::same_shape_as("final_image"));

  // bind an output image first and prefilter it
  // write the prefiltered result to 0th mip of downsample_image
  // start downsampling the mipped downsample_image
  //    -> for (int i = 1; i < lodCount; i++)
  //    ->    lod = i -1
  //    ->    bind i'th mip of downsample_image
  // start upsampling by writing to the last mip of upsample_image from downsample_image last mip
  //    -> for (int i = lodCount - 3; i >= 0; i--)
  //    ->    lod = i + 1
  //    ->    bind i'th mip of upsample_image
#ifdef BLOOM
  struct BloomPushConst {
    // x: threshold, y: clamp, z: radius, w: unused
    Vec4 Params = {};
    // x: Stage, y: Lod
    int Lod = 0;
  } bloom_push_const;

  bloom_push_const.Params.x = RendererConfig::get()->bloom_config.threshold;
  bloom_push_const.Params.y = RendererConfig::get()->bloom_config.clamp;

  rg->add_pass({
    .name = "bloom_prefilter",
    .resources = {"bloom_downsampled_image"_image >> vuk::eComputeRW >> "bloom_prefiltered_output", "pbr_output"_image >> vuk::eComputeSampled},
    .execute = [bloom_push_const](vuk::CommandBuffer& command_buffer) {
      command_buffer.bind_compute_pipeline("bloom_prefilter_pipeline")
                    .push_constants(vuk::ShaderStageFlagBits::eCompute, 0, bloom_push_const)
                    .bind_image(0, 0, "bloom_prefiltered_image")
                    .bind_sampler(0, 0, {})
                    .bind_image(0, 1, "pbr_output")
                    .bind_sampler(0, 1, {})
                    .dispatch((VulkanRenderer::get_viewport_width() + 8 - 1) / 8, (VulkanRenderer::get_viewport_height() + 8 - 1) / 8, 1);
    }
  });

  rg->attach_and_clear_image("bloom_downsampled_image", {.format = vk_context->swapchain->format, .sample_count = vuk::SampleCountFlagBits::e1, .level_count = 5, .layer_count = 1}, vuk::Black<float>);
  rg->inference_rule("bloom_downsampled_image", vuk::same_2D_extent_as("final_image"));

  rg->add_pass({
    .name = "bloom_downsample",
    .resources = {"bloom_prefiltered_output"_image >> vuk::eComputeRW >> "bloom_downsampled_output", "bloom_prefiltered_image"_image >> vuk::eComputeSampled},
    .execute = [bloom_push_const](vuk::CommandBuffer& command_buffer) {
      command_buffer.bind_compute_pipeline("bloom_downsample_pipeline")
                    .push_constants(vuk::ShaderStageFlagBits::eCompute, 0, bloom_push_const)
                    .bind_image(0, 0, "bloom_downsampled_image")
                    .bind_sampler(0, 0, {})
                    .bind_image(0, 1, "bloom_prefiltered_image")
                    .bind_sampler(0, 1, {})
                    .dispatch((VulkanRenderer::get_viewport_width() + 8 - 1) / 8, (VulkanRenderer::get_viewport_height() + 8 - 1) / 8, 1);
    }
  });
#endif
  auto [final_buff, final_buff_fut] = create_buffer(frame_allocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(&m_renderer_data.final_pass_data, 1));
  auto final_buffer = *final_buff;

  rg->add_pass({
    .name = "final_pass",
    .resources = {
      "final_image"_image >> vuk::eColorRW >> "final_output",
      "pbr_output"_image >> vuk::eFragmentSampled,
      "ssr_output"_image >> vuk::eFragmentSampled,
      "gtao_final_output"_image >> vuk::eFragmentSampled,
    },
    .execute = [final_buffer](vuk::CommandBuffer& command_buffer) {
      command_buffer.bind_graphics_pipeline("final_pipeline")
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .broadcast_color_blend(vuk::BlendPreset::eOff)
                    .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
                    .bind_sampler(0, 0, vuk::LinearSamplerClamped)
                    .bind_image(0, 0, "pbr_output")
                    .bind_sampler(0, 3, vuk::LinearSamplerClamped)
                    .bind_image(0, 3, "ssr_output")
                    .bind_sampler(0, 4, {})
                    .bind_image(0, 4, "gtao_final_output")
                    .bind_buffer(0, 5, final_buffer)
                    .draw(3, 1, 0, 0);
    }
  });

  auto final_image_attachment = vuk::ImageAttachment{
    .extent = dim,
    .format = vk_context->swapchain->format,
    .sample_count = vuk::Samples::e1,
    .level_count = 1,
    .layer_count = 1
  };
  rg->attach_and_clear_image("final_image", final_image_attachment, vuk::Black<float>);

  auto final_image_fut = vuk::Future{rg, vuk::Name("final_output")};

  if (RendererConfig::get()->fxaa_config.enabled)
    final_image_fut = apply_fxaa(final_image_fut, target);

  return create_scope<vuk::Future>(std::move(final_image_fut));
}

void DefaultRenderPipeline::update_skybox(const SceneRenderer::SkyboxLoadEvent& e) {
  m_resources.cube_map = e.cube_map;

  generate_prefilter();
}

vuk::Future DefaultRenderPipeline::apply_fxaa(vuk::Future source, vuk::Future dst) {
  std::unique_ptr<vuk::RenderGraph> rgp = std::make_unique<vuk::RenderGraph>("fxaa");
  rgp->attach_in("jagged", std::move(source));
  rgp->attach_in("smooth", std::move(dst));
  rgp->add_pass({
    .name = "fxaa",
    .resources = {"jagged"_image >> vuk::eFragmentSampled, "smooth"_image >> vuk::eColorWrite},
    .execute = [](vuk::CommandBuffer& command_buffer) {
      // TODO(hatrickek): make settings
      constexpr auto current_threshold = 0.0833f;
      constexpr auto relative_threshold = 0.125f;

      command_buffer.bind_graphics_pipeline("fxaa_pipeline")
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .broadcast_color_blend(vuk::BlendPreset::eOff)
                    .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
                    .specialize_constants(0, current_threshold).specialize_constants(1, relative_threshold)
                    .bind_image(0, 0, "jagged")
                    .bind_sampler(0, 0, {})
                    .draw(3, 1, 0, 0);
    }
  });
  rgp->inference_rule("jagged", vuk::same_shape_as("smooth"));
  rgp->inference_rule("smooth", vuk::same_shape_as("jagged"));
  return {std::move(rgp), "smooth+"};
}

void DefaultRenderPipeline::cascaded_shadow_pass(const Ref<vuk::RenderGraph> rg, vuk::Buffer& shadow_buffer) {
  Ref<vuk::RenderGraph> shadow_map = create_ref<vuk::RenderGraph>("shadow_map");
  const auto shadow_size = RendererConfig::get()->direct_shadows_config.size;
  const vuk::ImageAttachment shadow_array_attachment = {
    .extent = vuk::Dimension3D::absolute(shadow_size, shadow_size),
    .format = vuk::Format::eD16Unorm,
    .sample_count = vuk::SampleCountFlagBits::e1,
    .view_type = vuk::ImageViewType::e2DArray,
    .level_count = 1,
    .layer_count = 4
  };
  shadow_map->attach_and_clear_image("shadow_map_array", shadow_array_attachment, vuk::DepthOne);

  auto diverged_cascade_names = diverge_image_layers(shadow_map, "shadow_map_array", 4);

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
                         .depthCompareOp = vuk::CompareOp::eLessOrEqual, // try lessorequal
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
            VulkanRenderer::render_mesh(mesh, command_buffer, [](const Mesh::Primitive*) { return true; });
          }
        }
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

  auto [brdf_img, brdf_fut] = Prefilter::generate_brdflut();
  brdf_fut.wait(allocator, compiler);
  brdf_image = std::move(brdf_img);

  auto [irradiance_img, irradiance_fut] = Prefilter::generate_irradiance_cube(skybox_cube, m_resources.cube_map);
  irradiance_fut.wait(allocator, compiler);
  irradiance_image = std::move(irradiance_img);

  auto [prefilter_img, prefilter_fut] = Prefilter::generate_prefiltered_cube(skybox_cube, m_resources.cube_map);
  prefilter_fut.wait(allocator, compiler);
  prefiltered_image = std::move(prefilter_img);
}

void DefaultRenderPipeline::update_parameters(SceneRenderer::ProbeChangeEvent& e) {
  auto& ubo = m_renderer_data.final_pass_data;
  auto& component = e.probe;
  ubo.film_grain = {component.film_grain_enabled, component.film_grain_intensity};
  ubo.chromatic_aberration = {component.chromatic_aberration_enabled, component.chromatic_aberration_intensity};
  ubo.film_grain = {component.film_grain_enabled, component.film_grain_intensity};
  ubo.vignette_offset.w = component.vignette_enabled;
  ubo.vignette_color.a = component.vignette_intensity;
}

void DefaultRenderPipeline::update_final_pass_data(RendererConfig::ConfigChangeEvent& e) {
  auto& ubo = m_renderer_data.final_pass_data;
  ubo.tonemapper = RendererConfig::get()->color_config.tonemapper;
  ubo.exposure = RendererConfig::get()->color_config.exposure;
  ubo.gamma = RendererConfig::get()->color_config.gamma;
  ubo.enable_bloom = RendererConfig::get()->bloom_config.enabled;
  ubo.enable_ssr = RendererConfig::get()->ssr_config.enabled;

  // GTAO
  ubo.enable_ssao = RendererConfig::get()->gtao_config.enabled;

  memcpy(&gtao_settings, &RendererConfig::get()->gtao_config.settings, sizeof RendererConfig::GTAO::Settings);
}
}
