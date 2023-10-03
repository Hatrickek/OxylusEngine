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
}

void DefaultRenderPipeline::update(Scene* scene) {
  // TODO:(hatrickek) Temporary solution for camera.
  m_renderer_context.current_camera = VulkanRenderer::renderer_context.current_camera;

  if (!m_renderer_context.current_camera)
    OX_CORE_FATAL("No camera is set for rendering!");

  // Mesh
  {
    OX_SCOPED_ZONE_N("Mesh System");
    const auto view = scene->m_Registry.view<TransformComponent, MeshRendererComponent, MaterialComponent, TagComponent>();
    for (const auto&& [entity, transform, meshrenderer, material, tag] : view.each()) {
      auto e = Entity(entity, scene);
      auto parent = e.GetParent();
      bool parentEnabled = true;
      if (parent)
        parentEnabled = parent.GetComponent<TagComponent>().enabled;
      if (tag.enabled && parentEnabled && !material.materials.empty())
        mesh_draw_list.emplace_back(meshrenderer.mesh_geometry.get(), e.GetWorldTransform(), material.materials, meshrenderer.submesh_index);
    }
  }

  // Particle system
  {
    OX_SCOPED_ZONE_N("Particle System");
    const auto particleSystemView = scene->m_Registry.view<TransformComponent, ParticleSystemComponent>();
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
      std::vector<Entity> lights;
      const auto view = scene->m_Registry.view<LightComponent>();
      lights.reserve(view.size());
      for (auto&& [e, lc] : view.each()) {
        Entity entity = {e, scene};
        if (!entity.GetComponent<TagComponent>().enabled)
          continue;
        lights.emplace_back(entity);
      }
      scene_lights = std::move(lights);
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

  auto [par_buffer, par_buffer_fut] = create_buffer(*vk_context->superframe_allocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(&m_renderer_data.final_pass_data, 1));
  parameters_buffer = std::move(par_buffer);
  enqueue_future(std::move(par_buffer_fut));

  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileUtils::read_shader_file("DepthNormalPass.vert"),
      FileUtils::get_shader_path("DepthNormalPass.vert"));
    pci.add_glsl(FileUtils::read_shader_file("DepthNormalPass.frag"),
      FileUtils::get_shader_path("DepthNormalPass.frag"));
    vk_context->context->create_named_pipeline("DepthPrePassPipeline", pci);
  }
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileUtils::read_shader_file("DirectShadowDepthPass.vert"), "DirectShadowDepthPass.vert");
    pci.add_glsl(FileUtils::read_shader_file("DirectShadowDepthPass.frag"), "DirectShadowDepthPass.frag");
    vk_context->context->create_named_pipeline("ShadowPipeline", pci);
  }
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileUtils::read_shader_file("Skybox.vert"), "Skybox.vert");
    pci.add_glsl(FileUtils::read_shader_file("Skybox.frag"), "Skybox.frag");
    vk_context->context->create_named_pipeline("SkyboxPipeline", pci);
  }
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileUtils::read_shader_file("PBRTiled.vert"), FileUtils::get_shader_path("PBRTiled.vert"));
    pci.add_glsl(FileUtils::read_shader_file("PBRTiled.frag"), FileUtils::get_shader_path("PBRTiled.frag"));
    vk_context->context->create_named_pipeline("PBRPipeline", pci);
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
    vk_context->context->create_named_pipeline("SSRPipeline", pci);
  }
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileUtils::read_shader_file("FinalPass.vert"), FileUtils::get_shader_path("FinalPass.vert"));
    pci.add_glsl(FileUtils::read_shader_file("FinalPass.frag"), FileUtils::get_shader_path("FinalPass.frag"));
    vk_context->context->create_named_pipeline("FinalPipeline", pci);
  }
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_hlsl(FileUtils::read_shader_file("GTAO/GTAO_First.hlsl"), FileUtils::get_shader_path("GTAO/GTAO_First.hlsl"), vuk::HlslShaderStage::eCompute, "CSPrefilterDepths16x16");
    vk_context->context->create_named_pipeline("gtao_first_pipeline", pci);
  }
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_hlsl(FileUtils::read_shader_file("GTAO/GTAO_Main.hlsl"), FileUtils::get_shader_path("GTAO/GTAO_Main.hlsl"), vuk::HlslShaderStage::eCompute, "CSGTAOHigh");
    vk_context->context->create_named_pipeline("gtao_main_pipeline", pci);
  }
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_hlsl(FileUtils::read_shader_file("GTAO/GTAO_Final.hlsl"), FileUtils::get_shader_path("GTAO/GTAO_Final.hlsl"), vuk::HlslShaderStage::eCompute, "CSDenoisePass");
    vk_context->context->create_named_pipeline("gtao_final_pipeline", pci);
  }

  wait_for_futures(vk_context);
}

static uint32_t get_material_index(const std::unordered_map<uint32_t, uint32_t>& material_map,
                                   uint32_t mesh_index,
                                   uint32_t material_index) {
  uint32_t size = 0;
  for (uint32_t i = 0; i < mesh_index; i++)
    size += material_map.at(i);
  return size + material_index;
}

Scope<vuk::Future> DefaultRenderPipeline::on_render(vuk::Allocator& frame_allocator, const vuk::Future& target) {
  mesh_draw_list.clear();

  update(m_scene);

  const auto rg = create_ref<vuk::RenderGraph>("DefaultRenderPipelineRenderGraph");

  auto vk_context = VulkanContext::get();

  m_renderer_data.ubo_vs.view = m_renderer_context.current_camera->GetViewMatrix();
  m_renderer_data.ubo_vs.cam_pos = m_renderer_context.current_camera->GetPosition();
  m_renderer_data.ubo_vs.projection = m_renderer_context.current_camera->GetProjectionMatrixFlipped();
  auto [vs_buff, vs_buffer_fut] = create_buffer(frame_allocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(&m_renderer_data.ubo_vs, 1));
  auto& vs_buffer = *vs_buff;

  rg->attach_in("final_image", target);

  std::vector<Material::Parameters> materials = {};

  std::unordered_map<uint32_t, uint32_t> materialMap; //mesh, material

  for (uint32_t i = 0; i < static_cast<uint32_t>(mesh_draw_list.size()); i++) {
    for (auto& mat : mesh_draw_list[i].materials)
      materials.emplace_back(mat->parameters);
    materialMap.emplace(i, static_cast<uint32_t>(mesh_draw_list[i].materials.size()));
  }

  auto [matBuff, matBufferFut] = create_buffer(frame_allocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(materials));
  auto& mat_buffer = *matBuff;

  rg->add_pass({
    .name = "DepthPrePass",
    .resources = {
      "Normal_Image"_image >> vuk::eColorRW >> "normal_output",
      "Depth_Image"_image >> vuk::eDepthStencilRW >> "depth_output"
    },
    .execute = [this, vs_buffer, mat_buffer, materialMap](vuk::CommandBuffer& commandBuffer) {
      commandBuffer.set_viewport(0, vuk::Rect2D::framebuffer())
                   .set_scissor(0, vuk::Rect2D::framebuffer())
                   .broadcast_color_blend(vuk::BlendPreset::eOff)
                   .set_rasterization({.cullMode = vuk::CullModeFlagBits::eBack})
                   .set_depth_stencil(vuk::PipelineDepthStencilStateCreateInfo{
                      .depthTestEnable = true,
                      .depthWriteEnable = true,
                      .depthCompareOp = vuk::CompareOp::eLessOrEqual,
                    })
                   .bind_graphics_pipeline("DepthPrePassPipeline")
                   .bind_buffer(0, 0, vs_buffer)
                   .bind_buffer(2, 0, mat_buffer);

      for (uint32_t i = 0; i < mesh_draw_list.size(); i++) {
        auto& mesh = mesh_draw_list[i];

        VulkanRenderer::render_mesh(mesh,
          commandBuffer,
          [&mesh, &commandBuffer, i, materialMap](const Mesh::Primitive* part) {
            const auto& material = mesh.materials[part->material_index];

            commandBuffer.push_constants(vuk::ShaderStageFlagBits::eVertex | vuk::ShaderStageFlagBits::eFragment, 0, mesh.transform)
                         .push_constants(vuk::ShaderStageFlagBits::eFragment, sizeof(glm::mat4), get_material_index(materialMap, i, part->material_index))
                         .bind_sampler(1, 0, vuk::LinearSamplerRepeated)
                         .bind_image(1, 0, *material->normal_texture->get_texture().view)
                         .bind_sampler(1, 1, vuk::LinearSamplerRepeated)
                         .bind_image(1, 1, *material->metallic_roughness_texture->get_texture().view);
            return true;
          });
      }
    }
  });

  rg->attach_and_clear_image("Normal_Image", {.format = vuk::Format::eR16G16B16A16Sfloat, .sample_count = vuk::SampleCountFlagBits::e1}, vuk::Black<float>);
  rg->attach_and_clear_image("Depth_Image", {.format = vuk::Format::eD32Sfloat, .sample_count = vuk::SampleCountFlagBits::e1}, vuk::DepthOne);
  rg->inference_rule("Normal_Image", vuk::same_shape_as("final_image"));
  rg->inference_rule("Depth_Image", vuk::same_shape_as("final_image"));

#pragma region GTAO

  gtao_update_constants(gtao_constants, (int)VulkanRenderer::get_viewport_width(), (int)VulkanRenderer::get_viewport_height(), gtao_settings, value_ptr(m_renderer_data.ubo_vs.projection), false, vk_context->context->get_frame_count());

  auto [gtao_const_buff, gtao_const_buff_fut] = create_buffer(frame_allocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(&gtao_constants, 1));
  auto& gtao_const_buffer = *gtao_const_buff;

  rg->attach_and_clear_image("gtao_depth_image", {.format = vuk::Format::eR16G16B16A16Sfloat, .sample_count = vuk::SampleCountFlagBits::e1}, vuk::Black<float>);
  rg->inference_rule("gtao_depth_image", vuk::same_shape_as("final_image"));

  auto diverged_names = diverge_image(rg, "gtao_depth_image", 5);

  rg->add_pass({
    .name = "gtao_first_pass",
    .resources = {
      "gtao_depth_image_mip0"_image >> vuk::eComputeRW,
      "gtao_depth_image_mip1"_image >> vuk::eComputeRW,
      "gtao_depth_image_mip2"_image >> vuk::eComputeRW,
      "gtao_depth_image_mip3"_image >> vuk::eComputeRW,
      "gtao_depth_image_mip4"_image >> vuk::eComputeRW,
      "depth_output"_image >> vuk::eComputeSampled
    },
    .execute = [gtao_const_buffer](vuk::CommandBuffer& command_buffer) {
      command_buffer.bind_compute_pipeline("gtao_first_pipeline")
                    .bind_buffer(0, 0, gtao_const_buffer)
                    .bind_image(0, 1, "depth_output")
                    .bind_image(0, 2, "gtao_depth_image_mip0")
                    .bind_image(0, 3, "gtao_depth_image_mip1")
                    .bind_image(0, 4, "gtao_depth_image_mip2")
                    .bind_image(0, 5, "gtao_depth_image_mip3")
                    .bind_image(0, 6, "gtao_depth_image_mip4")
                    .bind_sampler(0, 7, vuk::LinearSamplerClamped)
                    .dispatch((VulkanRenderer::get_viewport_width() + 16 - 1) / 16, (VulkanRenderer::get_viewport_height() + 16 - 1) / 16);
    }
  });

  rg->converge_image_explicit(diverged_names, "gtao_depth_output");

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
                    .dispatch((VulkanRenderer::get_viewport_width() + 16 - 1) / 16, (VulkanRenderer::get_viewport_height() + 16 - 1) / 16);
    }
  });

  rg->attach_and_clear_image("gtao_main_image", {.format = vuk::Format::eR16G16B16A16Sfloat, .sample_count = vuk::SampleCountFlagBits::e1, .view_type = vuk::ImageViewType::e2D, .level_count = 1, .layer_count = 1}, vuk::Black<float>);
  rg->attach_and_clear_image("gtao_edge_image", {.format = vuk::Format::eR16G16B16A16Sfloat, .sample_count = vuk::SampleCountFlagBits::e1, .view_type = vuk::ImageViewType::e2D, .level_count = 1, .layer_count = 1}, vuk::Black<float>);
  rg->inference_rule("gtao_main_image", vuk::same_extent_as("final_image"));
  rg->inference_rule("gtao_edge_image", vuk::same_extent_as("final_image"));

  rg->add_pass({
    .name = "gtao_final_pass",
    .resources = {
      "gtao_final_image"_image >> vuk::eComputeRW >> "gtao_final_output",
      "gtao_main_output"_image >> vuk::eComputeSampled,
      "gtao_edge_output"_image >> vuk::eComputeSampled
    },
    .execute = [gtao_const_buffer](vuk::CommandBuffer& commandBuffer) {
      commandBuffer.bind_compute_pipeline("gtao_final_pipeline")
                   .bind_buffer(0, 0, gtao_const_buffer)
                   .bind_image(0, 1, "gtao_main_output")
                   .bind_image(0, 2, "gtao_edge_output")
                   .bind_image(0, 3, "gtao_final_image")
                   .bind_sampler(0, 4, vuk::LinearSamplerClamped)
                   .dispatch((VulkanRenderer::get_viewport_width() + 16 - 1) / 16, (VulkanRenderer::get_viewport_height() + 16 - 1) / 16);
    }
  });

  rg->attach_and_clear_image("gtao_final_image", {.format = vuk::Format::eR16G16B16A16Sfloat, .sample_count = vuk::SampleCountFlagBits::e1, .view_type = vuk::ImageViewType::e2D, .level_count = 1, .layer_count = 1}, vuk::Black<float>);
  rg->inference_rule("gtao_final_image", vuk::same_extent_as("final_image"));

#pragma endregion

  DirectShadowPass::DirectShadowUB direct_shadow_ub = {};

  auto [buffer, buffer_fut] = create_buffer(frame_allocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(&direct_shadow_ub, 1));
  auto& shadow_buffer = *buffer;

  rg->add_pass({
    .name = "DirectShadowPass",
    .resources = {"ShadowArray"_image >> vuk::eDepthStencilRW >> "ShadowArray_Output"},
    .execute = [this, shadow_buffer, &direct_shadow_ub](vuk::CommandBuffer& command_buffer) {
      const auto shadow_size = static_cast<float>(RendererConfig::get()->direct_shadows_config.size);
      command_buffer.bind_graphics_pipeline("ShadowPipeline")
                    .broadcast_color_blend({})
                    .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
                    .set_dynamic_state(vuk::DynamicStateFlagBits::eViewport | vuk::DynamicStateFlagBits::eScissor)
                    .set_depth_stencil(vuk::PipelineDepthStencilStateCreateInfo{
                       .depthTestEnable = true,
                       .depthWriteEnable = true,
                       .depthCompareOp = vuk::CompareOp::eGreaterOrEqual,
                     })
                    .set_viewport(0, vuk::Viewport{0, 0, shadow_size, shadow_size, 0, 1})
                    .set_scissor(0, vuk::Rect2D{vuk::Sizing::eAbsolute, {}, {static_cast<unsigned>(shadow_size), static_cast<unsigned>(shadow_size)}, {}})
                    .bind_buffer(0, 0, shadow_buffer);

      for (const auto& e : scene_lights) {
        const auto& light_component = e.GetComponent<LightComponent>();
        if (light_component.type != LightComponent::LightType::Directional)
          continue;

        DirectShadowPass::UpdateCascades(e, m_renderer_context.current_camera, direct_shadow_ub);

        for (const auto& mesh : mesh_draw_list) {
          struct PushConst {
            glm::mat4 model_matrix{};
            uint32_t cascade_index = 0;
          } push_const;
          push_const.model_matrix = mesh.transform;
          push_const.cascade_index = 0; //todo

          command_buffer.push_constants(vuk::ShaderStageFlagBits::eVertex, 0, &push_const);

          VulkanRenderer::render_mesh(mesh, command_buffer, [&](const Mesh::Primitive*) { return true; });
        }
      }
    }
  });

  auto shadow_size = RendererConfig::get()->direct_shadows_config.size;
  vuk::ImageAttachment shadow_array_attachment = {
    .extent = {
      .extent = {
        shadow_size, shadow_size,
        1
      }
    },
    .format = vuk::Format::eD16Unorm, .sample_count = vuk::SampleCountFlagBits::e1, .view_type = vuk::ImageViewType::e2DArray, .level_count = 4
  };
  rg->attach_and_clear_image("ShadowArray", shadow_array_attachment, vuk::DepthOne);

  auto [point_lights_buf, point_lights_buffer_fut] = create_buffer(frame_allocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(point_lights_data.data(), 1));
  auto point_lights_buffer = *point_lights_buf;

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
    .name = "GeomertyPass",
    .resources = {
      "PBR_Image"_image >> vuk::eColorRW >> "PBR_Output",
      "PBR_Depth"_image >> vuk::eDepthStencilRW,
      "ShadowArray_Output"_image >> vuk::eFragmentSampled
    },
    .execute = [this, vs_buffer, point_lights_buffer, shadow_buffer, pbr_buffer, mat_buffer, materialMap](
    vuk::CommandBuffer& commandBuffer) {
      // Skybox pass
      struct SkyboxPushConstant {
        Mat4 view;
        float lod_bias;
      } skybox_push_constant = {};

      skybox_push_constant.view = m_renderer_context.current_camera->SkyboxView;

      const auto sky_light_view = m_scene->m_Registry.view<SkyLightComponent>();
      for (auto&& [e, sc] : sky_light_view.each())
        skybox_push_constant.lod_bias = sc.lod_bias;

      commandBuffer.bind_graphics_pipeline("SkyboxPipeline")
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

      commandBuffer.bind_graphics_pipeline("PBRPipeline")
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
                   .bind_image(0, 7, "ShadowArray_Output")
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
              transparent_mesh_draw_list.emplace_back(mesh);
              return false;
            }

            commandBuffer.push_constants(vuk::ShaderStageFlagBits::eVertex | vuk::ShaderStageFlagBits::eFragment, 0, mesh.transform)
                         .push_constants(vuk::ShaderStageFlagBits::eFragment, sizeof(glm::mat4), get_material_index(materialMap, i, part->material_index));

            material->bind_textures(commandBuffer);
            return true;
          });
      }

      commandBuffer.broadcast_color_blend(vuk::BlendPreset::eAlphaBlend);

      // Transparency pass
      for (uint32_t i = 0; i < transparent_mesh_draw_list.size(); i++) {
        auto& mesh = transparent_mesh_draw_list[i];
        VulkanRenderer::render_mesh(mesh,
          commandBuffer,
          [&](const Mesh::Primitive* part) {
            const auto& material = mesh.materials[part->material_index];
            commandBuffer.push_constants(vuk::ShaderStageFlagBits::eVertex | vuk::ShaderStageFlagBits::eFragment, 0, mesh.transform)
                         .push_constants(vuk::ShaderStageFlagBits::eFragment, sizeof(glm::mat4), get_material_index(materialMap, i, part->material_index));

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

  rg->attach_and_clear_image("PBR_Image", {.format = vk_context->swapchain->format, .sample_count = vuk::SampleCountFlagBits::e1}, vuk::Black<float>);
  rg->attach_and_clear_image("PBR_Depth", {.format = vuk::Format::eD32Sfloat, .sample_count = vuk::SampleCountFlagBits::e1}, vuk::DepthOne);
  rg->inference_rule("PBR_Image", vuk::same_shape_as("final_image"));
  rg->inference_rule("PBR_Depth", vuk::same_shape_as("final_image"));

  struct SSRData {
    int samples = 30;
    int binary_search_samples = 8;
    float max_dist = 50.0f;
  } ssr_data;

  auto [ssr_buff, ssr_buff_fut] = create_buffer(frame_allocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(&ssr_data, 1));
  auto ssr_buffer = *ssr_buff;

  rg->add_pass({
    .name = "SSRPass",
    .resources = {
      "SSR_Image"_image >> vuk::eComputeRW >> "SSR_Output",
      "PBR_Output"_image >> vuk::eComputeSampled,
      "depth_output"_image >> vuk::eComputeSampled,
      "normal_output"_image >> vuk::eComputeSampled
    },
    .execute = [this, ssr_buffer, vs_buffer](vuk::CommandBuffer& command_buffer) {
      command_buffer.bind_compute_pipeline("SSRPipeline")
                    .bind_sampler(0, 0, vuk::LinearSamplerRepeated)
                    .bind_image(0, 0, "SSR_Image")
                    .bind_sampler(0, 1, vuk::LinearSamplerRepeated)
                    .bind_image(0, 1, "PBR_Output")
                    .bind_sampler(0, 2, vuk::LinearSamplerRepeated)
                    .bind_image(0, 2, "depth_output")
                    .bind_sampler(0, 3, vuk::LinearSamplerRepeated)
                    .bind_image(0, 3, m_resources.cube_map->as_attachment())
                    .bind_sampler(0, 4, vuk::LinearSamplerRepeated)
                    .bind_image(0, 4, "normal_output")
                    .bind_buffer(0, 5, vs_buffer)
                    .bind_buffer(0, 6, ssr_buffer)
                    .dispatch((VulkanRenderer::get_viewport_width() + 8 - 1) / 8, (VulkanRenderer::get_viewport_height() + 8 - 1) / 8, 1);
    }
  });

  rg->attach_and_clear_image("SSR_Image", {.sample_count = vuk::SampleCountFlagBits::e1}, vuk::Black<float>);
  rg->inference_rule("SSR_Image", vuk::same_shape_as("final_image"));
  rg->inference_rule("SSR_Image", vuk::same_format_as("final_image"));

  auto [final_buff, final_buff_fut] = create_buffer(frame_allocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(&m_renderer_data.final_pass_data, 1));
  auto final_buffer = *final_buff;

  rg->add_pass({
    .name = "FinalPass",
    .resources = {
      "final_image"_image >> vuk::eColorRW >> "Final_Output",
      "PBR_Output"_image >> vuk::eFragmentSampled,
      "gtao_final_output"_image >> vuk::eFragmentSampled,
    },
    .execute = [final_buffer](vuk::CommandBuffer& command_buffer) {
      command_buffer.bind_graphics_pipeline("FinalPipeline")
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .broadcast_color_blend(vuk::BlendPreset::eOff)
                    .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
                    .bind_sampler(0, 0, vuk::LinearSamplerRepeated)
                    .bind_image(0, 0, "PBR_Output")
                    .bind_sampler(0, 3, vuk::LinearSamplerRepeated)
                    .bind_image(0, 3, "SSR_Output")
                    .bind_sampler(0, 4, vuk::LinearSamplerRepeated)
                    .bind_image(0, 4, "gtao_final_output")
                    .bind_buffer(0, 5, final_buffer)
                    .draw(3, 1, 0, 0);
    }
  });

  return create_scope<vuk::Future>(vuk::Future{rg, vuk::Name("Final_Output")});
}

void DefaultRenderPipeline::update_skybox(const SceneRenderer::SkyboxLoadEvent& e) {
  m_resources.cube_map = e.cube_map;

  generate_prefilter();
}

void DefaultRenderPipeline::update_lighting_data() {
  OX_SCOPED_ZONE;
  for (auto& e : scene_lights) {
    auto& light_component = e.GetComponent<LightComponent>();
    auto& transform_component = e.GetComponent<TransformComponent>();
    switch (light_component.type) {
      case LightComponent::LightType::Directional: break;
      case LightComponent::LightType::Point: {
        point_lights_data.emplace_back(VulkanRenderer::LightingData{
          Vec4{transform_component.translation, light_component.intensity},
          Vec4{light_component.color, light_component.range}, Vec4{transform_component.rotation, 1.0f}
        });
      }
      break;
      case LightComponent::LightType::Spot: break;
    }
  }
}

void DefaultRenderPipeline::generate_prefilter() {
  OX_SCOPED_ZONE;
  vuk::Compiler compiler{};
  auto& allocator = *VulkanContext::get()->superframe_allocator;

  auto [brdf_img, brdf_fut] = Prefilter::GenerateBRDFLUT();
  brdf_fut.wait(allocator, compiler);
  brdf_image = std::move(brdf_img);

  auto [irradiance_img, irradiance_fut] = Prefilter::GenerateIrradianceCube(skybox_cube, m_resources.cube_map);
  irradiance_fut.wait(allocator, compiler);
  irradiance_image = std::move(irradiance_img);

  auto [prefilter_img, prefilter_fut] = Prefilter::GeneratePrefilteredCube(skybox_cube, m_resources.cube_map);
  prefilter_fut.wait(allocator, compiler);
  prefiltered_image = std::move(prefilter_img);
}

void DefaultRenderPipeline::update_parameters(SceneRenderer::ProbeChangeEvent& e) {
  std::vector<SceneRenderer::ProbeChangeEvent> test;
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
  ubo.enable_bloom = RendererConfig::get()->bloom_config.enabled;
  ubo.enable_ssr = RendererConfig::get()->ssr_config.enabled;
  ubo.enable_ssao = RendererConfig::get()->ssao_config.enabled;
}
}
