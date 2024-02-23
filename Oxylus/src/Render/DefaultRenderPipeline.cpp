#include "DefaultRenderPipeline.h"

#include <glm/gtc/type_ptr.inl>
#include <vuk/Partials.hpp>
#include <ankerl/unordered_dense.h>

#include "DebugRenderer.h"
#include "RendererCommon.h"
#include "SceneRendererEvents.h"

#include "Core/App.h"
#include "Assets/AssetManager.h"
#include "Scene/Entity.h"
#include "PBR/DirectShadowPass.h"
#include "PBR/Prefilter.h"

#include "Thread/TaskScheduler.h"

#include "Utils/CVars.h"
#include "Utils/FileUtils.h"
#include "Utils/Profiler.h"
#include "Utils/Timer.h"

#include "Vulkan/VukUtils.h"
#include "Vulkan/VulkanContext.h"

#include "Vulkan/Renderer.h"

namespace Ox {
static std::vector<uint32_t> cumulated_material_map = {};

static std::vector<uint32_t> cumulate_material_map(const ankerl::unordered_dense::map<uint32_t, uint32_t>& material_map) {
  OX_SCOPED_ZONE;
  std::vector<uint32_t> cumulative_sums;
  uint32_t sum = 0;
  for (const auto& entry : material_map) {
    sum += entry.second;
    cumulative_sums.emplace_back(sum);
  }
  return cumulative_sums;
}

static uint32_t get_material_index(const uint32_t mesh_index, const uint32_t material_index) {
  OX_SCOPED_ZONE;
  if (mesh_index == 0)
    return material_index;

  return cumulated_material_map[mesh_index - 1] + material_index;
}

VkDescriptorSetLayoutBinding bindless_binding(uint32_t binding, vuk::DescriptorType descriptor_type, uint32_t count = 1024) {
  return {
    .binding = binding,
    .descriptorType = (VkDescriptorType)descriptor_type,
    .descriptorCount = count,
    .stageFlags = (VkShaderStageFlags)vuk::ShaderStageFlagBits::eAll
  };
}

void DefaultRenderPipeline::init(vuk::Allocator& allocator) {
  OX_SCOPED_ZONE;

  Timer timer = {};

  auto& ctx = allocator.get_context();

  load_pipelines(allocator);

  if (initalized)
    return;

  ADD_TASK_TO_PIPE(
    this,
    this->m_quad = RendererCommon::generate_quad();
  );

  ADD_TASK_TO_PIPE(
    this,
    this->m_cube = RendererCommon::generate_cube();
  );

  create_static_textures(allocator);
  create_descriptor_sets(allocator, ctx);

  App::get_system<TaskScheduler>()->wait_for_all();

  initalized = true;

  OX_CORE_TRACE("DefaultRenderPipeline initialized: {} ms", timer.get_elapsed_ms());
}

void DefaultRenderPipeline::load_pipelines(vuk::Allocator& allocator) {
  vuk::PipelineBaseCreateInfo bindless_pci;
  vuk::DescriptorSetLayoutCreateInfo bindless_dslci_00 = {};
  bindless_dslci_00.bindings = {
    bindless_binding(0, vuk::DescriptorType::eUniformBuffer, 1),
    bindless_binding(1, vuk::DescriptorType::eUniformBuffer, 1),
    bindless_binding(2, vuk::DescriptorType::eStorageBuffer),
    bindless_binding(3, vuk::DescriptorType::eSampledImage),
    bindless_binding(4, vuk::DescriptorType::eSampledImage),
    bindless_binding(5, vuk::DescriptorType::eSampledImage, 8),
    bindless_binding(6, vuk::DescriptorType::eSampledImage, 8),
    bindless_binding(7, vuk::DescriptorType::eStorageImage),
    bindless_binding(8, vuk::DescriptorType::eSampledImage),
    bindless_binding(9, vuk::DescriptorType::eSampler, 4),
  };
  bindless_dslci_00.index = 0;
  for (int i = 0; i < 10; i++)
    bindless_dslci_00.flags.emplace_back(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
  bindless_pci.explicit_set_layouts.emplace_back(bindless_dslci_00);

  ADD_TASK_TO_PIPE(
    =,
    bindless_pci.add_hlsl(FileUtils::read_shader_file("DepthNormalPrePass.hlsl"), FileUtils::get_shader_path("DepthNormalPrePass.hlsl"), vuk::HlslShaderStage::eVertex, "VSmain");
    bindless_pci.add_hlsl(FileUtils::read_shader_file("DepthNormalPrePass.hlsl"), FileUtils::get_shader_path("DepthNormalPrePass.hlsl"), vuk::HlslShaderStage::ePixel, "PSmain");
    allocator.get_context().create_named_pipeline("depth_pre_pass_pipeline", bindless_pci);
  );

  ADD_TASK_TO_PIPE(
    =,
    bindless_pci.add_hlsl(FileUtils::read_shader_file("DirectionalShadowPass.hlsl"), FileUtils::get_shader_path("DirectionalShadowPass.hlsl"), vuk::HlslShaderStage::eVertex, "VSmain");
    allocator.get_context().create_named_pipeline("shadow_pipeline", bindless_pci);
  );

  ADD_TASK_TO_PIPE(
    =,
    bindless_pci.add_hlsl(FileUtils::read_shader_file("PBRForward.hlsl"), FileUtils::get_shader_path("PBRForward.hlsl"), vuk::HlslShaderStage::eVertex, "VSmain");
    bindless_pci.add_hlsl(FileUtils::read_shader_file("PBRForward.hlsl"), FileUtils::get_shader_path("PBRForward.hlsl"), vuk::HlslShaderStage::ePixel, "PSmain");
    allocator.get_context().create_named_pipeline("pbr_pipeline", bindless_pci);
  );
  ADD_TASK_TO_PIPE(
    =,
    bindless_pci.add_hlsl(FileUtils::read_shader_file("PBRForward.hlsl"), FileUtils::get_shader_path("PBRForward.hlsl"), vuk::HlslShaderStage::eVertex, "VSmain");
    bindless_pci.add_hlsl(FileUtils::read_shader_file("PBRForward.hlsl"), FileUtils::get_shader_path("PBRForward.hlsl"), vuk::HlslShaderStage::ePixel, "PSmain");
    bindless_pci.define("TRANSPARENT", "");
    allocator.get_context().create_named_pipeline("pbr_transparency_pipeline", bindless_pci);
  );
  ADD_TASK_TO_PIPE(
    =,
    bindless_pci.add_hlsl(FileUtils::read_shader_file("PostProcess/SSR.hlsl"), FileUtils::get_shader_path("PostProcess/SSR.hlsl"), vuk::HlslShaderStage::eCompute);
    allocator.get_context().create_named_pipeline("ssr_pipeline", bindless_pci);
  );
  ADD_TASK_TO_PIPE(
    &allocator,
    vuk::PipelineBaseCreateInfo pci;
    pci.add_hlsl(FileUtils::read_shader_file("FullscreenTriangle.hlsl"), FileUtils::get_shader_path("FullscreenTriangle.hlsl"), vuk::HlslShaderStage::eVertex);
    pci.add_glsl(FileUtils::read_shader_file("FinalPass.frag"), FileUtils::get_shader_path("FinalPass.frag"));
    allocator.get_context().create_named_pipeline("final_pipeline", pci);
  );

  // --- GTAO ---
  ADD_TASK_TO_PIPE(
    &allocator,
    vuk::PipelineBaseCreateInfo pci;
    pci.add_hlsl(FileUtils::read_shader_file("GTAO/GTAO_First.hlsl"), FileUtils::get_shader_path("GTAO/GTAO_First.hlsl"), vuk::HlslShaderStage::eCompute, "CSPrefilterDepths16x16");
    pci.define("XE_GTAO_FP32_DEPTHS", "");
    pci.define("XE_GTAO_USE_HALF_FLOAT_PRECISION", "0");
    pci.define("XE_GTAO_USE_DEFAULT_CONSTANTS", "0");
    allocator.get_context().create_named_pipeline("gtao_first_pipeline", pci);
  );
  ADD_TASK_TO_PIPE(
    &allocator,
    vuk::PipelineBaseCreateInfo pci;
    pci.add_hlsl(FileUtils::read_shader_file("GTAO/GTAO_Main.hlsl"), FileUtils::get_shader_path("GTAO/GTAO_Main.hlsl"), vuk::HlslShaderStage::eCompute, "CSGTAOHigh");
    pci.define("XE_GTAO_FP32_DEPTHS", "");
    pci.define("XE_GTAO_USE_HALF_FLOAT_PRECISION", "0");
    pci.define("XE_GTAO_USE_DEFAULT_CONSTANTS", "0");
    allocator.get_context().create_named_pipeline("gtao_main_pipeline", pci);
  );
  ADD_TASK_TO_PIPE(
    &allocator,
    vuk::PipelineBaseCreateInfo pci;
    pci.add_hlsl(FileUtils::read_shader_file("GTAO/GTAO_Final.hlsl"), FileUtils::get_shader_path("GTAO/GTAO_Final.hlsl"), vuk::HlslShaderStage::eCompute, "CSDenoisePass");
    pci.define("XE_GTAO_FP32_DEPTHS", "");
    pci.define("XE_GTAO_USE_HALF_FLOAT_PRECISION", "0");
    pci.define("XE_GTAO_USE_DEFAULT_CONSTANTS", "0");
    allocator.get_context().create_named_pipeline("gtao_denoise_pipeline", pci);
  );
  ADD_TASK_TO_PIPE(
    &allocator,
    vuk::PipelineBaseCreateInfo pci;
    pci.add_hlsl(FileUtils::read_shader_file("GTAO/GTAO_Final.hlsl"), FileUtils::get_shader_path("GTAO/GTAO_Final.hlsl"), vuk::HlslShaderStage::eCompute, "CSDenoiseLastPass");
    pci.define("XE_GTAO_FP32_DEPTHS", "");
    pci.define("XE_GTAO_USE_HALF_FLOAT_PRECISION", "0");
    pci.define("XE_GTAO_USE_DEFAULT_CONSTANTS", "0");
    allocator.get_context().create_named_pipeline("gtao_final_pipeline", pci);
  );

  ADD_TASK_TO_PIPE(
    &allocator,
    vuk::PipelineBaseCreateInfo pci;
    pci.add_hlsl(FileUtils::read_shader_file("FullscreenTriangle.hlsl"), FileUtils::get_shader_path("FullscreenTriangle.hlsl"), vuk::HlslShaderStage::eVertex);
    pci.add_glsl(FileUtils::read_shader_file("PostProcess/FXAA.frag"), FileUtils::get_shader_path("PostProcess/FXAA.frag"));
    allocator.get_context().create_named_pipeline("fxaa_pipeline", pci);
  );

  // --- Bloom ---
  ADD_TASK_TO_PIPE(
    &allocator,
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileUtils::read_shader_file("PostProcess/BloomPrefilter.comp"), FileUtils::get_shader_path("PostProcess/BloomPrefilter.comp"));
    allocator.get_context().create_named_pipeline("bloom_prefilter_pipeline", pci);
  );
  ADD_TASK_TO_PIPE(
    &allocator,
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileUtils::read_shader_file("PostProcess/BloomDownsample.comp"), FileUtils::get_shader_path("PostProcess/BloomDownsample.comp"));
    allocator.get_context().create_named_pipeline("bloom_downsample_pipeline", pci);
  );
  ADD_TASK_TO_PIPE(
    &allocator,
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileUtils::read_shader_file("PostProcess/BloomUpsample.comp"), FileUtils::get_shader_path("PostProcess/BloomUpsample.comp"));
    allocator.get_context().create_named_pipeline("bloom_upsample_pipeline", pci);
  );

  ADD_TASK_TO_PIPE(
    =,
    bindless_pci.add_hlsl(FileUtils::read_shader_file("Debug/Grid.hlsl"), FileUtils::get_shader_path("Debug/Grid.hlsl"), vuk::HlslShaderStage::eVertex);
    bindless_pci.add_hlsl(FileUtils::read_shader_file("Debug/Grid.hlsl"), FileUtils::get_shader_path("Debug/Grid.hlsl"), vuk::HlslShaderStage::ePixel, "PSmain");
    allocator.get_context().create_named_pipeline("grid_pipeline", bindless_pci);
  );
  ADD_TASK_TO_PIPE(
    &allocator,
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileUtils::read_shader_file("Debug/Unlit.vert"), FileUtils::get_shader_path("Debug/Unlit.vert"));
    pci.add_glsl(FileUtils::read_shader_file("Debug/Unlit.frag"), FileUtils::get_shader_path("Debug/Unlit.frag"));
    allocator.get_context().create_named_pipeline("unlit_pipeline", pci);
  );

  // --- Atmosphere ---
  ADD_TASK_TO_PIPE(
    =,
    bindless_pci.add_hlsl(FileUtils::read_shader_file("Atmosphere/TransmittanceLUT.hlsl"), FileUtils::get_shader_path("Atmosphere/TransmittanceLUT.hlsl"), vuk::HlslShaderStage::eCompute);
    allocator.get_context().create_named_pipeline("sky_transmittance_pipeline", bindless_pci);
  );
  ADD_TASK_TO_PIPE(
    =,
    bindless_pci.add_hlsl(FileUtils::read_shader_file("Atmosphere/MultiScatterLUT.hlsl"), FileUtils::get_shader_path("Atmosphere/MultiScatterLUT.hlsl"), vuk::HlslShaderStage::eCompute);
    allocator.get_context().create_named_pipeline("sky_multiscatter_pipeline", bindless_pci);
  );
  ADD_TASK_TO_PIPE(
    =,
    bindless_pci.add_hlsl(FileUtils::read_shader_file("FullscreenTriangle.hlsl"), FileUtils::get_shader_path("FullscreenTriangle.hlsl"), vuk::HlslShaderStage::eVertex);
    bindless_pci.add_hlsl(FileUtils::read_shader_file("Atmosphere/SkyView.hlsl"), FileUtils::get_shader_path("Atmosphere/SkyView.hlsl"), vuk::HlslShaderStage::ePixel);
    allocator.get_context().create_named_pipeline("sky_view_pipeline", bindless_pci);
  );
  ADD_TASK_TO_PIPE(
    =,
    bindless_pci.add_hlsl(FileUtils::read_shader_file("FullscreenTriangle.hlsl"), FileUtils::get_shader_path("FullscreenTriangle.hlsl"), vuk::HlslShaderStage::eVertex);
    bindless_pci.add_hlsl(FileUtils::read_shader_file("Atmosphere/SkyViewFinal.hlsl"), FileUtils::get_shader_path("Atmosphere/SkyViewFinal.hlsl"), vuk::HlslShaderStage::ePixel);
    allocator.get_context().create_named_pipeline("sky_view_final_pipeline", bindless_pci);
  );
  ADD_TASK_TO_PIPE(
    =,
    bindless_pci.add_hlsl(FileUtils::read_shader_file("Atmosphere/SkyEnvMap.hlsl"), FileUtils::get_shader_path("Atmosphere/SkyEnvMap.hlsl"), vuk::HlslShaderStage::eVertex, "VSmain");
    bindless_pci.add_hlsl(FileUtils::read_shader_file("Atmosphere/SkyEnvMap.hlsl"), FileUtils::get_shader_path("Atmosphere/SkyEnvMap.hlsl"), vuk::HlslShaderStage::ePixel, "PSmain");
    allocator.get_context().create_named_pipeline("sky_envmap_pipeline", bindless_pci);
  );

  wait_for_futures(allocator);

  App::get_system<TaskScheduler>()->wait_for_all();
}

void DefaultRenderPipeline::commit_descriptor_sets(vuk::Allocator& allocator) {
  OX_SCOPED_ZONE;
  auto& ctx = allocator.get_context();

  scene_data.num_lights = (int)scene_lights.size();
  scene_data.grid_max_distance = RendererCVar::cvar_draw_grid_distance.get();
  scene_data.screen_size = IVec2(Renderer::get_viewport_width(), Renderer::get_viewport_height());

  const auto capture_projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
  const Mat4 capture_views[] = {
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f)),
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(-1.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f)),
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f)),
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f)),
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), Vec3(0.0f, -1.0f, 0.0f)),
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f), Vec3(0.0f, -1.0f, 0.0f))
  };

  for (int i = 0; i < 6; i++)
    scene_data.cubemap_view_projections[i] = capture_projection * capture_views[i];

  scene_data.cascade_splits = direct_shadow_ub.cascade_splits;
  scene_data.scissor_normalized = direct_shadow_ub.scissor_normalized;
  memcpy(scene_data.cascade_view_projections, direct_shadow_ub.cascade_view_proj_mat, sizeof(Mat4) * 4);

  scene_data.indices.pbr_image_index = PBR_IMAGE_INDEX;
  scene_data.indices.normal_image_index = NORMAL_IMAGE_INDEX;
  scene_data.indices.depth_image_index = DEPTH_IMAGE_INDEX;
  scene_data.indices.sky_transmittance_lut_index = SKY_TRANSMITTANCE_LUT_INDEX;
  scene_data.indices.sky_multiscatter_lut_index = SKY_MULTISCATTER_LUT_INDEX;
  scene_data.indices.sky_envmap_index = SKY_ENVMAP_INDEX;
  scene_data.indices.shadow_array_index = SHADOW_ARRAY_INDEX;
  scene_data.indices.gtao_buffer_image_index = GTAO_BUFFER_IMAGE_INDEX;
  scene_data.indices.ssr_buffer_image_index = SSR_BUFFER_IMAGE_INDEX;
  scene_data.indices.lights_buffer_index = LIGHTS_BUFFER_INDEX;
  scene_data.indices.materials_buffer_index = MATERIALS_BUFFER_INDEX;

  scene_data.post_processing_data.tonemapper = RendererCVar::cvar_tonemapper.get();
  scene_data.post_processing_data.exposure = RendererCVar::cvar_exposure.get();
  scene_data.post_processing_data.gamma = RendererCVar::cvar_gamma.get();
  scene_data.post_processing_data.enable_bloom = RendererCVar::cvar_bloom_enable.get();
  scene_data.post_processing_data.enable_ssr = RendererCVar::cvar_ssr_enable.get();
  scene_data.post_processing_data.enable_gtao = RendererCVar::cvar_gtao_enable.get();

  auto [scene_buff, scene_buff_fut] = create_cpu_buffer(allocator, std::span(&scene_data, 1));
  const auto& scene_buffer = *scene_buff;

  CameraData camera_data = {
    .position = Vec4(current_camera->get_position(), 0.0f),
    .projection_matrix = current_camera->get_projection_matrix(),
    .inv_projection_matrix = inverse(current_camera->get_projection_matrix()),
    .view_matrix = current_camera->get_view_matrix(),
    .inv_view_matrix = inverse(current_camera->get_view_matrix()),
    .inv_projection_view_matrix = inverse(current_camera->get_projection_matrix() * current_camera->get_view_matrix()),
    .near_clip = current_camera->get_near(),
    .far_clip = current_camera->get_far(),
    .fov = current_camera->get_fov(),
  };

  auto [camera_buff, camera_buff_fut] = create_cpu_buffer(allocator, std::span(&camera_data, 1));
  const auto& camera_buffer = *camera_buff;

  // fill it if it's empty. Or we could allow vulkan to bind null buffers with VK_EXT_robustness2 nullDescriptor feature. 
  if (scene_lights.empty())
    scene_lights.emplace_back();

  std::vector<LightData> light_datas;
  light_datas.reserve(scene_lights.size());

  std::transform(scene_lights.begin(),
                 scene_lights.end(),
                 std::back_inserter(light_datas),
                 [](const std::pair<LightData, LightComponent>& pair) { return pair.first; });

  auto [lights_buff, lights_buff_fut] = create_cpu_buffer(allocator, std::span(light_datas));
  const auto& lights_buffer = *lights_buff;

  auto [ssr_buff, ssr_buff_fut] = create_cpu_buffer(allocator, std::span(&ssr_data, 1));
  auto ssr_buffer = *ssr_buff;

  std::vector<Material::Parameters> material_parameters = {};
  for (auto& mesh : mesh_component_list) {
    auto& materials = mesh.materials; //mesh.mesh_base->get_materials(mesh.node_index);
    for (auto& mat : materials) {
      material_parameters.emplace_back(mat->parameters);

      if (mat->get_albedo_texture() && mat->get_albedo_texture()->is_valid_id())
        descriptor_set_00->update_sampled_image(8, mat->get_albedo_texture()->get_id(), *mat->get_albedo_texture()->get_texture().view, vuk::ImageLayout::eReadOnlyOptimalKHR);
      if (mat->get_normal_texture() && mat->get_normal_texture()->is_valid_id())
        descriptor_set_00->update_sampled_image(8, mat->get_normal_texture()->get_id(), *mat->get_normal_texture()->get_texture().view, vuk::ImageLayout::eReadOnlyOptimalKHR);
      if (mat->get_physical_texture() && mat->get_physical_texture()->is_valid_id())
        descriptor_set_00->update_sampled_image(8, mat->get_physical_texture()->get_id(), *mat->get_physical_texture()->get_texture().view, vuk::ImageLayout::eReadOnlyOptimalKHR);
      if (mat->get_ao_texture() && mat->get_ao_texture()->is_valid_id())
        descriptor_set_00->update_sampled_image(8, mat->get_ao_texture()->get_id(), *mat->get_ao_texture()->get_texture().view, vuk::ImageLayout::eReadOnlyOptimalKHR);
      if (mat->get_emissive_texture() && mat->get_emissive_texture()->is_valid_id())
        descriptor_set_00->update_sampled_image(8, mat->get_emissive_texture()->get_id(), *mat->get_emissive_texture()->get_texture().view, vuk::ImageLayout::eReadOnlyOptimalKHR);
    }
  }

  if (material_parameters.empty())
    material_parameters.emplace_back();

  auto [matBuff, matBufferFut] = create_cpu_buffer(allocator, std::span(material_parameters));
  auto& mat_buffer = *matBuff;

  descriptor_set_00->update_uniform_buffer(0, 0, scene_buffer);
  descriptor_set_00->update_uniform_buffer(1, 0, camera_buffer);
  descriptor_set_00->update_storage_buffer(2, LIGHTS_BUFFER_INDEX, lights_buffer);
  descriptor_set_00->update_storage_buffer(2, MATERIALS_BUFFER_INDEX, mat_buffer);
  descriptor_set_00->update_storage_buffer(2, SSR_BUFFER_IMAGE_INDEX, ssr_buffer);

  // scene textures
  descriptor_set_00->update_sampled_image(3, PBR_IMAGE_INDEX, *pbr_texture.view, vuk::ImageLayout::eReadOnlyOptimalKHR);
  descriptor_set_00->update_sampled_image(3, NORMAL_IMAGE_INDEX, *normal_texture.view, vuk::ImageLayout::eReadOnlyOptimalKHR);
  descriptor_set_00->update_sampled_image(3, DEPTH_IMAGE_INDEX, *depth_texture.view, vuk::ImageLayout::eReadOnlyOptimalKHR);
  //descriptor_set_00->update_sampled_image(3, SSR_BUFFER_IMAGE_INDEX, *ssr_texture.view, vuk::ImageLayout::eReadOnlyOptimalKHR);

  descriptor_set_00->update_sampled_image(3, SKY_TRANSMITTANCE_LUT_INDEX, *sky_transmittance_lut.view, vuk::ImageLayout::eReadOnlyOptimalKHR);
  descriptor_set_00->update_sampled_image(3, SKY_MULTISCATTER_LUT_INDEX, *sky_multiscatter_lut.view, vuk::ImageLayout::eReadOnlyOptimalKHR);

  // scene uint texture array
  descriptor_set_00->update_sampled_image(4, GTAO_BUFFER_IMAGE_INDEX, *gtao_final_texture.view, vuk::ImageLayout::eReadOnlyOptimalKHR);

  // scene cubemap texture array
  descriptor_set_00->update_sampled_image(5, SKY_ENVMAP_INDEX, *sky_envmap_texture.view, vuk::ImageLayout::eReadOnlyOptimalKHR);

  // scene array texture array
  descriptor_set_00->update_sampled_image(6, SHADOW_ARRAY_INDEX, *sun_shadow_texture.view, vuk::ImageLayout::eReadOnlyOptimalKHR);

  // scene Read/Write textures
  descriptor_set_00->update_storage_image(7, SSR_BUFFER_IMAGE_INDEX, *ssr_texture.view);
  descriptor_set_00->update_storage_image(7, SKY_TRANSMITTANCE_LUT_INDEX, *sky_transmittance_lut.view);
  descriptor_set_00->update_storage_image(7, SKY_MULTISCATTER_LUT_INDEX, *sky_multiscatter_lut.view);

  descriptor_set_00->commit(ctx);
}

void DefaultRenderPipeline::create_static_textures(vuk::Allocator& allocator) {
  constexpr auto transmittance_lut_size = vuk::Extent3D{256, 64, 1};
  sky_transmittance_lut = create_texture(allocator, transmittance_lut_size, vuk::Format::eR16G16B16A16Sfloat, DEFAULT_USAGE_FLAGS | vuk::ImageUsageFlagBits::eStorage);

  constexpr auto multi_scatter_lut_size = vuk::Extent3D{32, 32, 1};
  sky_multiscatter_lut = create_texture(allocator, multi_scatter_lut_size, vuk::Format::eR16G16B16A16Sfloat, DEFAULT_USAGE_FLAGS | vuk::ImageUsageFlagBits::eStorage);

  const auto shadow_size = vuk::Extent3D{(unsigned)RendererCVar::cvar_shadows_size.get(), (unsigned)RendererCVar::cvar_shadows_size.get(), 1};
  const vuk::ImageAttachment shadow_attachment = {
    .image_flags = vuk::ImageCreateFlagBits::e2DArrayCompatible,
    .usage = DEFAULT_USAGE_FLAGS | vuk::ImageUsageFlagBits::eDepthStencilAttachment,
    .extent = vuk::Dimension3D{vuk::Sizing::eAbsolute, shadow_size, {}},
    .format = vuk::Format::eD32Sfloat,
    .sample_count = vuk::Samples::e1,
    .view_type = vuk::ImageViewType::e2DArray,
    .base_level = 0,
    .level_count = 1,
    .base_layer = 0,
    .layer_count = 4
  };

  sun_shadow_texture = create_texture(allocator, shadow_attachment);

  const vuk::ImageAttachment env_attachment = {
    .image_flags = vuk::ImageCreateFlagBits::eCubeCompatible,
    .usage = DEFAULT_USAGE_FLAGS | vuk::ImageUsageFlagBits::eColorAttachment,
    .extent = vuk::Dimension3D::absolute(512, 512, 1),
    .format = vuk::Format::eR16G16B16A16Sfloat,
    .sample_count = vuk::Samples::e1,
    .view_type = vuk::ImageViewType::eCube,
    .base_level = 0,
    .level_count = 1,
    .base_layer = 0,
    .layer_count = 6
  };

  sky_envmap_texture = create_texture(allocator, env_attachment);
}

void DefaultRenderPipeline::create_dynamic_textures(vuk::Allocator& allocator, const vuk::Dimension3D& dim) {
  if (gtao_final_texture.extent != dim.extent)
    gtao_final_texture = create_texture(allocator, dim.extent, vuk::Format::eR8Uint, DEFAULT_USAGE_FLAGS | vuk::ImageUsageFlagBits::eStorage);
  if (ssr_texture.extent != dim.extent)
    ssr_texture = create_texture(allocator, dim.extent, vuk::Format::eR32G32B32A32Sfloat, DEFAULT_USAGE_FLAGS | vuk::ImageUsageFlagBits::eStorage);
  if (pbr_texture.extent != dim.extent)
    pbr_texture = create_texture(allocator, dim.extent, vuk::Format::eR32G32B32A32Sfloat, DEFAULT_USAGE_FLAGS | vuk::ImageUsageFlagBits::eColorAttachment);
  if (normal_texture.extent != dim.extent)
    normal_texture = create_texture(allocator, dim.extent, vuk::Format::eR16G16B16A16Sfloat, DEFAULT_USAGE_FLAGS | vuk::ImageUsageFlagBits::eColorAttachment);
  if (depth_texture.extent != dim.extent)
    depth_texture = create_texture(allocator, dim.extent, vuk::Format::eD32Sfloat, DEFAULT_USAGE_FLAGS | vuk::ImageUsageFlagBits::eDepthStencilAttachment);
}

void DefaultRenderPipeline::create_descriptor_sets(vuk::Allocator& allocator, vuk::Context& ctx) {
  descriptor_set_00 = ctx.create_persistent_descriptorset(allocator, *ctx.get_named_pipeline("pbr_pipeline"), 0, 64);

  const vuk::Sampler linear_sampler_clamped = ctx.acquire_sampler(vuk::LinearSamplerClamped, ctx.get_frame_count());
  const vuk::Sampler linear_sampler_repeated = ctx.acquire_sampler(vuk::LinearSamplerRepeated, ctx.get_frame_count());
  const vuk::Sampler nearest_sampler_clamped = ctx.acquire_sampler(vuk::NearestSamplerClamped, ctx.get_frame_count());
  const vuk::Sampler nearest_sampler_repeated = ctx.acquire_sampler(vuk::NearestSamplerRepeated, ctx.get_frame_count());
  descriptor_set_00->update_sampler(9, 0, linear_sampler_clamped);
  descriptor_set_00->update_sampler(9, 1, linear_sampler_repeated);
  descriptor_set_00->update_sampler(9, 2, nearest_sampler_clamped);
  descriptor_set_00->update_sampler(9, 3, nearest_sampler_repeated);
}

void DefaultRenderPipeline::on_dispatcher_events(EventDispatcher& dispatcher) {
  dispatcher.sink<SkyboxLoadEvent>().connect<&DefaultRenderPipeline::update_skybox>(*this);
  dispatcher.sink<ProbeChangeEvent>().connect<&DefaultRenderPipeline::update_parameters>(*this);
}

void DefaultRenderPipeline::on_register_render_object(const MeshComponent& render_object) {
  OX_SCOPED_ZONE;

  if (!current_camera)
    return;

  {
    OX_SCOPED_ZONE_N("Frustum culling");
    const auto camera_frustum = current_camera->get_frustum();
    const auto* node = render_object.mesh_base->linear_nodes[render_object.node_index];
    if (!render_object.aabb.is_on_frustum(camera_frustum)) {
      Renderer::get_stats().drawcall_culled_count += (uint32_t)node->mesh_data->primitives.size();
      return;
    }
  }

  render_queue.add((uint32_t)mesh_component_list.size(), 0, distance(current_camera->get_position(), render_object.aabb.get_center()), 0);
  mesh_component_list.emplace_back(render_object);
}

void DefaultRenderPipeline::on_register_light(const TransformComponent& transform, const LightComponent& light) {
  OX_SCOPED_ZONE;
  auto& light_emplaced = scene_lights.emplace_back(
    LightData{
      .position = {transform.position},
      .intensity = light.intensity,
      .color = light.color,
      .radius = light.range,
      .rotation = transform.rotation,
      .type = (uint32_t)light.type
    },
    light
  );
  if (light.type == LightComponent::LightType::Directional)
    dir_light_data = &light_emplaced;
}

void DefaultRenderPipeline::on_register_camera(Camera* camera) {
  OX_SCOPED_ZONE;
  current_camera = camera;
}

void DefaultRenderPipeline::shutdown() {}

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

Unique<vuk::Future> DefaultRenderPipeline::on_render(vuk::Allocator& frame_allocator, const vuk::Future& target, vuk::Dimension3D dim) {
  OX_SCOPED_ZONE;
  if (!current_camera) {
    OX_CORE_ERROR("No camera is set for rendering!");
    // set a temporary one
    if (!default_camera)
      default_camera = create_shared<Camera>();
    current_camera = default_camera.get();
  }

  auto vk_context = VulkanContext::get();

  {
    OX_SCOPED_ZONE_N("Mesh Opaque Sorting")
    render_queue.sort_opaque();
  }

  ankerl::unordered_dense::map<uint32_t, uint32_t> material_map; //mesh, material

  for (auto& batch : render_queue.batches) {
    material_map.emplace(batch.mesh_index, mesh_component_list[batch.mesh_index].mesh_base->get_material_count());
  }

  cumulated_material_map = cumulate_material_map(material_map);

  const auto rg = create_shared<vuk::RenderGraph>("DefaultRenderPipelineRenderGraph");

  Vec3 sun_direction = {};
  Vec3 sun_color = {};

  if (dir_light_data) {
    sun_direction = normalize(dir_light_data->first.rotation);
    sun_color = dir_light_data->first.color * dir_light_data->first.intensity;
  }

  scene_data.sun_direction = normalize(sun_direction);
  scene_data.sun_color = Vec4(sun_color, 1.0f);

  create_dynamic_textures(*vk_context->superframe_allocator, dim);
  commit_descriptor_sets(frame_allocator);

  if (dir_light_data && dir_light_data->second.cast_shadows) {
    DirectShadowPass::update_cascades(sun_direction, current_camera, &direct_shadow_ub);
    auto attachment = vuk::ImageAttachment::from_texture(sun_shadow_texture);
    attachment.image_flags = vuk::ImageCreateFlagBits::e2DArrayCompatible;
    attachment.view_type = vuk::ImageViewType::e2DArray;
    rg->attach_and_clear_image("shadow_map_array", attachment, vuk::DepthOne);
    cascaded_shadow_pass(rg);
  }
  else {
    rg->attach_and_clear_image("shadow_array_output", vuk::ImageAttachment::from_texture(sun_shadow_texture), vuk::DepthOne);
  }

  dir_light_data = nullptr;
  scene_lights.clear();

  rg->attach_and_clear_image("black_image", {.format = vk_context->swapchain->format, .sample_count = vuk::SampleCountFlagBits::e1}, vuk::Black<float>);
  rg->inference_rule("black_image", vuk::same_shape_as("final_image"));

  rg->attach_and_clear_image("black_image_uint", {.format = vuk::Format::eR8Uint, .sample_count = vuk::SampleCountFlagBits::e1}, vuk::Black<float>);
  rg->inference_rule("black_image_uint", vuk::same_shape_as("final_image"));

  depth_pre_pass(rg);

  if (RendererCVar::cvar_gtao_enable.get())
    gtao_pass(frame_allocator, rg);

  sky_transmittance_pass(rg);
  sky_multiscatter_pass(rg);
  sky_view_lut_pass(rg);

  sky_envmap_pass(rg);

  geometry_pass(rg);

  if (RendererCVar::cvar_ssr_enable.get())
    ssr_pass(rg);

  if (RendererCVar::cvar_bloom_enable.get())
    bloom_pass(rg);

  auto [final_buff, final_buff_fut] = create_cpu_buffer(frame_allocator, std::span(&scene_data.post_processing_data, 1));
  auto& final_buffer = *final_buff;

  auto [bloom_resource, bloom_name] = get_attachment_or_black("bloom_upsampled_image", RendererCVar::cvar_bloom_enable.get());

  vuk::Name pbr_image_name = "pbr_output";

  if (RendererCVar::cvar_fxaa_enable.get()) {
    struct FXAAData {
      Vec2 inverse_screen_size;
    } fxaa_data;

    fxaa_data.inverse_screen_size = 1.0f / glm::vec2(Renderer::get_viewport_width(), Renderer::get_viewport_height());
    auto [fxaa_buff, fxaa_buffer_fut] = create_cpu_buffer(frame_allocator, std::span(&fxaa_data, 1));
    auto& fxaa_buffer = *fxaa_buff;

    rg->attach_and_clear_image("fxaa_image", {.format = vuk::Format::eR32G32B32A32Sfloat, .sample_count = vuk::Samples::e1}, vuk::Black<float>);
    rg->inference_rule("fxaa_image", vuk::same_shape_as("final_image"));

    apply_fxaa(rg.get(), pbr_image_name, "fxaa_image", fxaa_buffer);
    pbr_image_name = "fxaa_image+";
  }

  if (RendererCVar::cvar_enable_debug_renderer.get()) {
    debug_pass(rg, pbr_image_name, "depth_output", frame_allocator);
    pbr_image_name = pbr_image_name.append("+");
  }

  if (RendererCVar::cvar_draw_grid.get()) {
    apply_grid(rg.get(), pbr_image_name, "depth_output");
    pbr_image_name = pbr_image_name.append("+");
  }

  std::vector final_resources = {
    "final_image"_image >> vuk::eColorRW >> "final_output",
    vuk::Resource{pbr_image_name, vuk::Resource::Type::eImage, vuk::eFragmentSampled},
    bloom_resource,
  };

  rg->attach_in("final_image", target);

  rg->add_pass({
    .name = "final_pass",
    .resources = std::move(final_resources),
    .execute = [this, final_buffer, bloom_name, pbr_image_name](vuk::CommandBuffer& command_buffer) {
      render_queue.clear();
      mesh_component_list.clear();

      command_buffer.bind_graphics_pipeline("final_pipeline")
                    .set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .broadcast_color_blend(vuk::BlendPreset::eOff)
                    .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
                    .bind_sampler(0, 0, vuk::LinearSamplerClamped)
                    .bind_image(0, 0, pbr_image_name)
                    .bind_sampler(0, 4, {})
                    .bind_image(0, 4, bloom_name)
                    .bind_buffer(0, 5, final_buffer)
                    .draw(3, 1, 0, 0);
    }
  });

  return create_unique<vuk::Future>(rg, "final_output");
}

void DefaultRenderPipeline::sky_transmittance_pass(const Shared<vuk::RenderGraph>& rg) {
  OX_SCOPED_ZONE;

  IVec2 lut_size = {256, 64};

  rg->attach_and_clear_image("sky_transmittance_lut", vuk::ImageAttachment::from_texture(sky_transmittance_lut), vuk::Black<float>);

  rg->add_pass({
    .name = "sky_transmittance_lut_pass",
    .resources = {
      "sky_transmittance_lut"_image >> vuk::eComputeRW,
    },
    .execute = [this, lut_size](vuk::CommandBuffer& command_buffer) {
      command_buffer.bind_persistent(0, *descriptor_set_00)
                     //.bind_persistent(1, *descriptor_set_01)
                    .bind_compute_pipeline("sky_transmittance_pipeline")
                    .dispatch((lut_size.x + 8 - 1) / 8, (lut_size.y + 8 - 1) / 8);
    }
  });
}

void DefaultRenderPipeline::sky_multiscatter_pass(const Shared<vuk::RenderGraph>& rg) {
  OX_SCOPED_ZONE;

  IVec2 lut_size = {32, 32};

  rg->attach_and_clear_image("sky_multiscatter_lut", vuk::ImageAttachment::from_texture(sky_multiscatter_lut), vuk::Black<float>);

  rg->add_pass({
    .name = "sky_multiscatter_lut_pass",
    .resources = {
      "sky_multiscatter_lut"_image >> vuk::eComputeRW,
      "sky_transmittance_lut+"_image >> vuk::eComputeSampled,
    },
    .execute = [this, lut_size](vuk::CommandBuffer& command_buffer) {
      command_buffer.bind_compute_pipeline("sky_multiscatter_pipeline")
                    .bind_persistent(0, *descriptor_set_00)
                    .dispatch(lut_size.x, lut_size.y);
    }
  });
}

void DefaultRenderPipeline::update_skybox(const SkyboxLoadEvent& e) {
  OX_SCOPED_ZONE;
  cube_map = e.cube_map;

  if (cube_map)
    generate_prefilter(*VulkanContext::get()->superframe_allocator);
}

void DefaultRenderPipeline::depth_pre_pass(const Shared<vuk::RenderGraph>& rg) {
  OX_SCOPED_ZONE;

  rg->attach_and_clear_image("normal_image", vuk::ImageAttachment::from_texture(normal_texture), vuk::Black<float>);
  rg->attach_and_clear_image("depth_image", vuk::ImageAttachment::from_texture(depth_texture), vuk::DepthOne);

  rg->add_pass({
    .name = "depth_pre_pass",
    .resources = {
      "normal_image"_image >> vuk::eColorRW >> "normal_output",
      "depth_image"_image >> vuk::eDepthStencilRW >> "depth_output"
    },
    .execute = [this](vuk::CommandBuffer& command_buffer) {
      command_buffer.bind_persistent(0, *descriptor_set_00)
                    .set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .broadcast_color_blend(vuk::BlendPreset::eOff)
                    .set_rasterization({.cullMode = vuk::CullModeFlagBits::eBack})
                    .set_depth_stencil(vuk::PipelineDepthStencilStateCreateInfo{
                       .depthTestEnable = true,
                       .depthWriteEnable = true,
                       .depthCompareOp = vuk::CompareOp::eLessOrEqual,
                     })
                    .bind_graphics_pipeline("depth_pre_pass_pipeline");

      for (const auto& batch : render_queue.batches) {
        const auto& mesh = mesh_component_list[batch.mesh_index];
        mesh.mesh_base->bind_index_buffer(command_buffer);

        const auto node = mesh.mesh_base->linear_nodes[mesh.node_index];
        for (const auto primitive : node->mesh_data->primitives) {
          if (mesh.materials[primitive->material_index]->parameters.alpha_mode != (uint32_t)Material::AlphaMode::Opaque) {
            continue;
          }

          const auto material_index = get_material_index(batch.mesh_index, primitive->material_index);
          const auto pc = ShaderPC{mesh.transform, mesh.mesh_base->vertex_buffer->device_address, material_index};
          command_buffer.push_constants(vuk::ShaderStageFlagBits::eVertex | vuk::ShaderStageFlagBits::eFragment, 0, pc);

          Renderer::draw_indexed(command_buffer, primitive->index_count, 1, primitive->first_index, 0, 0);
        }
      }
    }
  });
}

void DefaultRenderPipeline::geometry_pass(const Shared<vuk::RenderGraph>& rg) {
  OX_SCOPED_ZONE;

  rg->attach_and_clear_image("pbr_image", vuk::ImageAttachment::from_texture(pbr_texture), vuk::Black<float>);

  auto [gtao_resource, gtao_name] = get_attachment_or_black_uint("gtao_final_output", RendererCVar::cvar_gtao_enable.get());

  const auto resources = std::vector<vuk::Resource>{
    "pbr_image"_image >> vuk::eColorRW >> "pbr_output",
    "depth_output"_image >> vuk::eDepthStencilRead,
    "shadow_array_output"_image >> vuk::eFragmentSampled,
    "sky_transmittance_lut+"_image >> vuk::eFragmentSampled,
    "sky_multiscatter_lut+"_image >> vuk::eFragmentSampled,
    "sky_envmap_image+"_image >> vuk::eFragmentSampled,
    gtao_resource
  };

  rg->add_pass({
    .name = "geometry_pass",
    .resources = resources,
    .execute = [this](vuk::CommandBuffer& command_buffer) {
      command_buffer.bind_persistent(0, *descriptor_set_00)
                    .set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .broadcast_color_blend(vuk::BlendPreset::eOff)
                    .set_depth_stencil({
                       .depthTestEnable = false,
                       .depthWriteEnable = false,
                       .depthCompareOp = vuk::CompareOp::eLessOrEqual
                     })
                    .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
                    .bind_graphics_pipeline("sky_view_final_pipeline")
                    .draw(3, 1, 0, 0);

      command_buffer.bind_graphics_pipeline("pbr_pipeline")
                    .bind_persistent(0, *descriptor_set_00)
                    .set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
                    .set_rasterization({.cullMode = vuk::CullModeFlagBits::eBack}).
                     set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .broadcast_color_blend({})
                    .set_depth_stencil(vuk::PipelineDepthStencilStateCreateInfo{
                       .depthTestEnable = true,
                       .depthWriteEnable = false,
                       .depthCompareOp = vuk::CompareOp::eLessOrEqual,
                     });

      for (const auto& batch : render_queue.batches) {
        const auto& mesh = mesh_component_list[batch.mesh_index];

        mesh.mesh_base->bind_index_buffer(command_buffer);

        const auto node = mesh.mesh_base->linear_nodes[mesh.node_index];
        for (const auto primitive : node->mesh_data->primitives) {
          if (mesh.materials[primitive->material_index]->parameters.alpha_mode == (uint32_t)Material::AlphaMode::Blend) {
            continue;
          }

          const auto material_index = get_material_index(batch.mesh_index, primitive->material_index);
          const auto pc = ShaderPC{mesh.transform, mesh.mesh_base->vertex_buffer->device_address, material_index};

          command_buffer.push_constants(vuk::ShaderStageFlagBits::eVertex | vuk::ShaderStageFlagBits::eFragment, 0, pc);

          Renderer::draw_indexed(command_buffer, primitive->index_count, 1, primitive->first_index, 0, 0);
        }
      }

      command_buffer.bind_graphics_pipeline("pbr_transparency_pipeline")
                    .broadcast_color_blend(vuk::BlendPreset::eAlphaBlend)
                    .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone});
      {
        OX_SCOPED_ZONE_N("Mesh Transparent Sorting")
        render_queue.sort_transparent();
      }

      for (const auto& batch : render_queue.batches) {
        const auto& mesh = mesh_component_list[batch.mesh_index];

        mesh.mesh_base->bind_index_buffer(command_buffer);

        const auto node = mesh.mesh_base->linear_nodes[mesh.node_index];
        for (const auto primitive : node->mesh_data->primitives) {
          if (mesh.materials[primitive->material_index]->parameters.alpha_mode != (uint32_t)Material::AlphaMode::Blend) {
            continue;
          }
          const auto material_index = get_material_index(batch.mesh_index, primitive->material_index);
          const auto pc = ShaderPC{mesh.transform, mesh.mesh_base->vertex_buffer->device_address, material_index};

          command_buffer.push_constants(vuk::ShaderStageFlagBits::eVertex | vuk::ShaderStageFlagBits::eFragment, 0, pc);

          Renderer::draw_indexed(command_buffer, primitive->index_count, 1, primitive->first_index, 0, 0);
        }
      }
    }
  });
}

void DefaultRenderPipeline::bloom_pass(const Shared<vuk::RenderGraph>& rg) {
  OX_SCOPED_ZONE;
  struct BloomPushConst {
    // x: threshold, y: clamp, z: radius, w: unused
    Vec4 params = {};
  } bloom_push_const;

  bloom_push_const.params.x = RendererCVar::cvar_bloom_threshold.get();
  bloom_push_const.params.y = RendererCVar::cvar_bloom_clamp.get();

  constexpr uint32_t bloom_mip_count = 8;

  rg->attach_and_clear_image("bloom_image", {.format = vuk::Format::eR32G32B32A32Sfloat, .sample_count = vuk::SampleCountFlagBits::e1, .level_count = bloom_mip_count, .layer_count = 1}, vuk::Black<float>);
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

  rg->attach_and_clear_image("bloom_upsample_image", {.format = vuk::Format::eR32G32B32A32Sfloat, .sample_count = vuk::SampleCountFlagBits::e1, .level_count = bloom_mip_count - 1, .layer_count = 1}, vuk::Black<float>);
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

void DefaultRenderPipeline::gtao_pass(vuk::Allocator& frame_allocator, const Shared<vuk::RenderGraph>& rg) {
  OX_SCOPED_ZONE;
  gtao_settings.QualityLevel = RendererCVar::cvar_gtao_quality_level.get();
  gtao_settings.DenoisePasses = RendererCVar::cvar_gtao_denoise_passes.get();
  gtao_settings.Radius = RendererCVar::cvar_gtao_radius.get();
  gtao_settings.RadiusMultiplier = 1.0f;
  gtao_settings.FalloffRange = RendererCVar::cvar_gtao_falloff_range.get();
  gtao_settings.SampleDistributionPower = RendererCVar::cvar_gtao_sample_distribution_power.get();
  gtao_settings.ThinOccluderCompensation = RendererCVar::cvar_gtao_thin_occluder_compensation.get();
  gtao_settings.FinalValuePower = RendererCVar::cvar_gtao_final_value_power.get();
  gtao_settings.DepthMIPSamplingOffset = RendererCVar::cvar_gtao_depth_mip_sampling_offset.get();

  gtao_update_constants(gtao_constants, (int)Renderer::get_viewport_width(), (int)Renderer::get_viewport_height(), gtao_settings, current_camera, 0);

  auto [gtao_const_buff, gtao_const_buff_fut] = create_cpu_buffer(frame_allocator, std::span(&gtao_constants, 1));
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
      .execute = [gtao_const_buffer, attachment_name, read_input](vuk::CommandBuffer& command_buffer) {
        command_buffer.bind_compute_pipeline("gtao_denoise_pipeline")
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

  rg->attach_and_clear_image("gtao_final_image", vuk::ImageAttachment::from_texture(gtao_final_texture), vuk::Black<float>);
  rg->inference_rule("gtao_final_image", vuk::same_extent_as("final_image"));
}

void DefaultRenderPipeline::ssr_pass(const Shared<vuk::RenderGraph>& rg) {
  OX_SCOPED_ZONE;
  rg->attach_and_clear_image("ssr_image", vuk::ImageAttachment::from_texture(ssr_texture), vuk::Black<float>);

  rg->add_pass({
    .name = "ssr_pass",
    .resources = {
      "ssr_image"_image >> vuk::eComputeRW >> "ssr_output",
      "pbr_output"_image >> vuk::eComputeSampled,
      "depth_output"_image >> vuk::eComputeSampled,
      "normal_output"_image >> vuk::eComputeSampled
    },
    .execute = [this](vuk::CommandBuffer& command_buffer) {
      command_buffer.bind_compute_pipeline("ssr_pipeline")
                    .bind_persistent(0, *descriptor_set_00)
                    .dispatch((Renderer::get_viewport_width() + 8 - 1) / 8, (Renderer::get_viewport_height() + 8 - 1) / 8, 1);
    }
  });
}

void DefaultRenderPipeline::apply_fxaa(vuk::RenderGraph* rg, const vuk::Name src, const vuk::Name dst, vuk::Buffer& fxaa_buffer) {
  OX_SCOPED_ZONE;
  rg->add_pass({
    .name = "fxaa",
    .resources = {
      vuk::Resource(dst, vuk::Resource::Type::eImage, vuk::eColorRW, dst.append("+")),
      vuk::Resource(src, vuk::Resource::Type::eImage, vuk::eFragmentSampled),
    },
    .execute = [fxaa_buffer, src](vuk::CommandBuffer& command_buffer) {
      command_buffer.bind_graphics_pipeline("fxaa_pipeline")
                    .set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .broadcast_color_blend(vuk::BlendPreset::eOff)
                    .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
                    .bind_image(0, 0, src)
                    .bind_sampler(0, 0, vuk::LinearSamplerClamped)
                    .bind_buffer(0, 1, fxaa_buffer)
                    .draw(3, 1, 0, 0);
    }
  });
}

void DefaultRenderPipeline::apply_grid(vuk::RenderGraph* rg, const vuk::Name dst, const vuk::Name depth_image_name) {
  OX_SCOPED_ZONE;
  rg->add_pass({
    .name = "grid",
    .resources = {
      vuk::Resource(dst, vuk::Resource::Type::eImage, vuk::eColorWrite, dst.append("+")),
      vuk::Resource(depth_image_name, vuk::Resource::Type::eImage, vuk::eDepthStencilRW)
    },
    .execute = [this](vuk::CommandBuffer& command_buffer) {
      command_buffer.bind_graphics_pipeline("grid_pipeline")
                    .set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .broadcast_color_blend(vuk::BlendPreset::eAlphaBlend)
                    .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
                    .set_depth_stencil({
                       .depthTestEnable = true,
                       .depthWriteEnable = false,
                       .depthCompareOp = vuk::CompareOp::eLessOrEqual
                     })
                    .bind_persistent(0, *descriptor_set_00);

      m_quad->bind_index_buffer(command_buffer)
            ->bind_vertex_buffer(command_buffer);

      Renderer::draw_indexed(command_buffer, m_quad->index_count, 1, 0, 0, 0);
    }
  });
}

void DefaultRenderPipeline::cascaded_shadow_pass(const Shared<vuk::RenderGraph>& rg) {
  OX_SCOPED_ZONE;
  auto [d_cascade_names, d_cascade_name_outputs] = diverge_image_layers(rg, "shadow_map_array", 4);

  for (uint32_t cascade_index = 0; cascade_index < SHADOW_MAP_CASCADE_COUNT; cascade_index++) {
    rg->add_pass({
      .name = "direct_shadow_pass",
      .resources = {
        vuk::Resource(d_cascade_names[cascade_index], vuk::Resource::Type::eImage, vuk::Access::eDepthStencilRW, d_cascade_name_outputs[cascade_index])
      },
      .execute = [this, cascade_index](vuk::CommandBuffer& command_buffer) {
        command_buffer.bind_persistent(0, *descriptor_set_00)
                      .bind_graphics_pipeline("shadow_pipeline")
                      .set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
                      .broadcast_color_blend({})
                      .set_rasterization({.depthClampEnable = true, .cullMode = vuk::CullModeFlagBits::eBack})
                      .set_depth_stencil(vuk::PipelineDepthStencilStateCreateInfo{
                         .depthTestEnable = true,
                         .depthWriteEnable = true,
                         .depthCompareOp = vuk::CompareOp::eLessOrEqual,
                       })
                      .set_viewport(0, vuk::Rect2D::framebuffer())
                      .set_scissor(0, vuk::Rect2D::framebuffer());

        for (const auto& batch : render_queue.batches) {
          const auto& mesh = mesh_component_list[batch.mesh_index];
          if (!mesh.cast_shadows)
            continue;

          mesh.mesh_base->bind_index_buffer(command_buffer);

          const auto node = mesh.mesh_base->linear_nodes[mesh.node_index];
          for (const auto primitive : node->mesh_data->primitives) {
            const ShaderPC pc = {mesh.transform, mesh.mesh_base->vertex_buffer->device_address, cascade_index};
            command_buffer.push_constants(vuk::ShaderStageFlagBits::eVertex, 0, pc);

            Renderer::draw_indexed(command_buffer, primitive->index_count, 1, primitive->first_index, 0, 0);
          }
        }
      }
    });
  }

  rg->converge_image_explicit(d_cascade_name_outputs, "shadow_array_output");
}

void DefaultRenderPipeline::generate_prefilter(vuk::Allocator& allocator) {
  OX_SCOPED_ZONE;
  vuk::Compiler compiler{};

  auto [brdf_img, brdf_fut] = Prefilter::generate_brdflut();
  brdf_fut.wait(allocator, compiler);
  brdf_texture = std::move(brdf_img);

  auto [irradiance_img, irradiance_fut] = Prefilter::generate_irradiance_cube(m_cube, cube_map);
  irradiance_fut.wait(allocator, compiler);
  irradiance_texture = std::move(irradiance_img);

  auto [prefilter_img, prefilter_fut] = Prefilter::generate_prefiltered_cube(m_cube, cube_map);
  prefilter_fut.wait(allocator, compiler);
  prefiltered_texture = std::move(prefilter_img);
}

void DefaultRenderPipeline::update_parameters(ProbeChangeEvent& e) {
  OX_SCOPED_ZONE;
  auto& ubo = scene_data.post_processing_data;
  auto& component = e.probe;
  ubo.film_grain = {component.film_grain_enabled, component.film_grain_intensity};
  ubo.chromatic_aberration = {component.chromatic_aberration_enabled, component.chromatic_aberration_intensity};
  ubo.film_grain = {component.film_grain_enabled, component.film_grain_intensity};
  ubo.vignette_offset.w = component.vignette_enabled;
  ubo.vignette_color.a = component.vignette_intensity;
}

void DefaultRenderPipeline::sky_envmap_pass(const Shared<vuk::RenderGraph>& rg) {
  auto attachment = vuk::ImageAttachment::from_texture(sky_envmap_texture);
  attachment.image_flags = vuk::ImageCreateFlagBits::eCubeCompatible;
  attachment.view_type = vuk::ImageViewType::eCube;
  rg->attach_and_clear_image("sky_envmap_image", attachment, vuk::Black<float>);

  rg->add_pass({
    .name = "sky_envmap_pass",
    .resources = {
      "sky_envmap_image"_image >> vuk::eColorRW,
    },
    .execute = [this](vuk::CommandBuffer& command_buffer) {
      command_buffer.bind_persistent(0, *descriptor_set_00)
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .broadcast_color_blend(vuk::BlendPreset::eOff)
                    .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
                    .set_depth_stencil({})
                    .bind_graphics_pipeline("sky_envmap_pipeline");

      m_cube->bind_vertex_buffer(command_buffer);
      m_cube->bind_index_buffer(command_buffer);
      command_buffer.draw_indexed(m_cube->index_count, 6, 0, 0, 0);
    }
  });
}

void DefaultRenderPipeline::sky_view_lut_pass(const Shared<vuk::RenderGraph>& rg) {
  OX_SCOPED_ZONE;

  const auto attachment = vuk::ImageAttachment{
    .extent = vuk::Dimension3D::absolute(192, 104, 1),
    .format = vuk::Format::eR16G16B16A16Sfloat,
    .sample_count = vuk::SampleCountFlagBits::e1,
    .base_level = 0,
    .level_count = 1,
    .base_layer = 0,
    .layer_count = 1
  };

  rg->attach_and_clear_image("sky_view_lut", attachment, vuk::Black<float>);

  rg->add_pass({
    .name = "sky_view_pass",
    .resources = {
      "sky_view_lut"_image >> vuk::eColorRW,
      "sky_transmittance_lut+"_image >> vuk::eFragmentSampled,
      "sky_multiscatter_lut+"_image >> vuk::eFragmentSampled,
    },
    .execute = [this](vuk::CommandBuffer& command_buffer) {
      command_buffer.bind_persistent(0, *descriptor_set_00)
                    .set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .broadcast_color_blend(vuk::BlendPreset::eOff)
                    .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
                    .bind_graphics_pipeline("sky_view_pipeline")
                    .draw(3, 1, 0, 0);
    }
  });
}

void DefaultRenderPipeline::debug_pass(const Shared<vuk::RenderGraph>& rg, const vuk::Name dst, const char* depth, vuk::Allocator& frame_allocator) const {
  OX_SCOPED_ZONE;
  struct DebugPassData {
    Mat4 vp = {};
    Mat4 model = {};
    Vec4 color = {};
  };

  const auto* camera = current_camera;
  std::vector<DebugPassData> buffers = {};

  DebugPassData data = {
    .vp = camera->get_projection_matrix() * camera->get_view_matrix(),
    .model = glm::identity<Mat4>(),
    .color = Vec4(0, 1, 0, 1)
  };

  buffers.emplace_back(data);

  auto [buff, buff_fut] = create_cpu_buffer(frame_allocator, std::span(buffers));
  auto& buffer = *buff;

  const auto& lines = DebugRenderer::get_instance()->get_lines(false);
  auto [vertices, index_count] = DebugRenderer::get_vertices_from_lines(lines);

  if (vertices.empty())
    vertices.emplace_back(Vertex{});

  auto [v_buff, v_buff_fut] = create_cpu_buffer(frame_allocator, std::span(vertices));
  auto& vertex_buffer = *v_buff;

  const auto& lines_dt = DebugRenderer::get_instance()->get_lines(true);
  auto [vertices_dt, index_count_dt] = DebugRenderer::get_vertices_from_lines(lines_dt);

  if (vertices_dt.empty())
    vertices_dt.emplace_back(Vertex{});

  auto [vd_buff, vd_buff_fut] = create_cpu_buffer(frame_allocator, std::span(vertices_dt));
  auto& vertex_buffer_dt = *vd_buff;

  auto& index_buffer = *DebugRenderer::get_instance()->get_global_index_buffer();

  rg->add_pass({
    .name = "debug_shapes_pass",
    .resources = {
      vuk::Resource(dst, vuk::Resource::Type::eImage, vuk::eColorWrite, dst.append("+")),
      vuk::Resource(depth, vuk::Resource::Type::eImage, vuk::eDepthStencilRead)
    },
    .execute = [this, buffer, index_count, index_count_dt, vertex_buffer, vertex_buffer_dt, index_buffer](vuk::CommandBuffer& command_buffer) {
      const auto vertex_layout = vuk::Packed{
        vuk::Format::eR32G32B32A32Sfloat,
        vuk::Format::eR32G32B32A32Sfloat,
        vuk::Ignore{sizeof(Vertex) - (sizeof(Vec4) + sizeof(Vec4))}
      };

      // not depth tested
      command_buffer.bind_graphics_pipeline("unlit_pipeline")
                    .set_depth_stencil(vuk::PipelineDepthStencilStateCreateInfo{
                       .depthTestEnable = false,
                       .depthWriteEnable = false,
                       .depthCompareOp = vuk::CompareOp::eLessOrEqual,
                     })
                    .set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
                    .broadcast_color_blend({})
                    .set_rasterization({.polygonMode = vuk::PolygonMode::eLine, .cullMode = vuk::CullModeFlagBits::eNone})
                    .set_primitive_topology(vuk::PrimitiveTopology::eLineList)
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .bind_buffer(0, 0, buffer)
                    .push_constants(vuk::ShaderStageFlagBits::eVertex, 0, 0)
                    .bind_vertex_buffer(0, vertex_buffer, 0, vertex_layout)
                    .bind_index_buffer(index_buffer, vuk::IndexType::eUint32);

      Renderer::draw_indexed(command_buffer, index_count, 1, 0, 0, 0);

      command_buffer.bind_graphics_pipeline("unlit_pipeline")
                    .set_depth_stencil(vuk::PipelineDepthStencilStateCreateInfo{
                       .depthTestEnable = true,
                       .depthWriteEnable = false,
                       .depthCompareOp = vuk::CompareOp::eLessOrEqual,
                     })
                    .set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
                    .broadcast_color_blend({})
                    .set_rasterization({.polygonMode = vuk::PolygonMode::eLine, .cullMode = vuk::CullModeFlagBits::eNone})
                    .set_primitive_topology(vuk::PrimitiveTopology::eLineList)
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .bind_buffer(0, 0, buffer)
                    .push_constants(vuk::ShaderStageFlagBits::eVertex, 0, 0)
                    .bind_vertex_buffer(0, vertex_buffer_dt, 0, vertex_layout)
                    .bind_index_buffer(index_buffer, vuk::IndexType::eUint32);

      Renderer::draw_indexed(command_buffer, index_count_dt, 1, 0, 0, 0);
    }
  });

  DebugRenderer::reset(true);
}
}
