#include "DefaultRenderPipeline.h"

#include <glm/gtc/type_ptr.inl>
#include <vuk/Partials.hpp>

#include "DebugRenderer.h"
#include "RendererCommon.h"
#include "SceneRendererEvents.h"

#include "Core/Application.h"
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

namespace Oxylus {
static std::vector<uint32_t> cumulated_material_map = {};

static std::vector<uint32_t> cumulate_material_map(const std::unordered_map<uint32_t, uint32_t>& material_map) {
  OX_SCOPED_ZONE;
  std::vector<uint32_t> cumulative_sums;
  uint32_t sum = 0;
  for (const auto& entry : material_map) {
    sum += entry.second;
    cumulative_sums.push_back(sum);
  }
  return cumulative_sums;
}

static uint32_t get_material_index(const uint32_t mesh_index, const uint32_t material_index) {
  OX_SCOPED_ZONE;
  if (mesh_index == 0)
    return 0;

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

  // Lights data
  scene_lights.reserve(MAX_NUM_LIGHTS);

  // Mesh data
  mesh_draw_list.reserve(MAX_NUM_MESHES);

  create_images(allocator);

  create_descriptor_sets(allocator, ctx);

  TaskScheduler::wait_for_all();

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
    bindless_binding(3, vuk::DescriptorType::eStorageBuffer),
    bindless_binding(4, vuk::DescriptorType::eSampledImage),
    bindless_binding(5, vuk::DescriptorType::eSampledImage),
    bindless_binding(6, vuk::DescriptorType::eSampledImage),
    bindless_binding(7, vuk::DescriptorType::eSampledImage),
    bindless_binding(8, vuk::DescriptorType::eSampledImage),
    bindless_binding(9, vuk::DescriptorType::eStorageImage),
  };
  bindless_dslci_00.index = 0;
  bindless_dslci_00.flags = {
    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
  };
  bindless_pci.explicit_set_layouts.emplace_back(bindless_dslci_00);

  vuk::DescriptorSetLayoutCreateInfo bindless_dslci_01 = {};
  bindless_dslci_01.bindings = {
    bindless_binding(0, vuk::DescriptorType::eSampler, 4),
  };
  bindless_dslci_01.index = 1;
  bindless_pci.explicit_set_layouts.emplace_back(bindless_dslci_01);

  ADD_TASK_TO_PIPE(
    =,
    bindless_pci.add_hlsl(FileUtils::read_shader_file("DepthNormalPrePass.hlsl"), FileUtils::get_shader_path("DepthNormalPrePass.hlsl"), vuk::HlslShaderStage::eVertex, "VSmain");
    bindless_pci.add_hlsl(FileUtils::read_shader_file("DepthNormalPrePass.hlsl"), FileUtils::get_shader_path("DepthNormalPrePass.hlsl"), vuk::HlslShaderStage::ePixel, "PSmain");
    allocator.get_context().create_named_pipeline("depth_pre_pass_pipeline", bindless_pci);
  );

  ADD_TASK_TO_PIPE(
    =,
    bindless_pci.add_hlsl(FileUtils::read_shader_file("DirectionalShadowPass.hlsl"), FileUtils::get_shader_path("DirectionalShadowPass.hlsl"), vuk::HlslShaderStage::eVertex, "VSmain");
    bindless_pci.add_hlsl(FileUtils::read_shader_file("DirectionalShadowPass.hlsl"), FileUtils::get_shader_path("DirectionalShadowPass.hlsl"), vuk::HlslShaderStage::ePixel, "PSmain");
    allocator.get_context().create_named_pipeline("shadow_pipeline", bindless_pci);
  );

  ADD_TASK_TO_PIPE(
    =,
    bindless_pci.add_hlsl(FileUtils::read_shader_file("Skybox.hlsl"), FileUtils::get_shader_path("Skybox.hlsl"), vuk::HlslShaderStage::eVertex, "VSmain");
    bindless_pci.add_hlsl(FileUtils::read_shader_file("Skybox.hlsl"), FileUtils::get_shader_path("Skybox.hlsl"), vuk::HlslShaderStage::ePixel, "PSmain");
    allocator.get_context().create_named_pipeline("skybox_pipeline", bindless_pci);
  );

  ADD_TASK_TO_PIPE(
    =,
    bindless_pci.add_hlsl(FileUtils::read_shader_file("PBRForward.hlsl"), FileUtils::get_shader_path("PBRForward.hlsl"), vuk::HlslShaderStage::eVertex, "VSmain");
    bindless_pci.add_hlsl(FileUtils::read_shader_file("PBRForward.hlsl"), FileUtils::get_shader_path("PBRForward.hlsl"), vuk::HlslShaderStage::ePixel, "PSmain");
    allocator.get_context().create_named_pipeline("pbr_pipeline", bindless_pci);
  );
#if 0
  ADD_TASK_TO_PIPE(
    &allocator,
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileUtils::read_shader_file("PBRForward.vert"), FileUtils::get_shader_path("PBRForward.vert"));
    pci.add_glsl(FileUtils::read_shader_file("PBRForward.frag"), FileUtils::get_shader_path("PBRForward.frag"));
    pci.define("CUBEMAP_PIPELINE", "");
    allocator.get_context().create_named_pipeline("pbr_cubemap_pipeline", pci);
  );
  ADD_TASK_TO_PIPE(
    &allocator,
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileUtils::read_shader_file("PBRForward.vert"), FileUtils::get_shader_path("PBRForward.vert"));
    pci.add_glsl(FileUtils::read_shader_file("PBRForward.frag"), FileUtils::get_shader_path("PBRForward.frag"));
    pci.define("BLEND_MODE", "");
    allocator.get_context().create_named_pipeline("pbr_transparency_pipeline", pci);
  );
  ADD_TASK_TO_PIPE(
    &allocator,
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileUtils::read_shader_file("PBRForward.vert"), FileUtils::get_shader_path("PBRForward.vert"));
    pci.add_glsl(FileUtils::read_shader_file("PBRForward.frag"), FileUtils::get_shader_path("PBRForward.frag"));
    pci.define("BLEND_MODE", "");
    allocator.get_context().create_named_pipeline("pbr_transparency_pipeline", pci);
  );
  ADD_TASK_TO_PIPE(
    &allocator,
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileUtils::read_shader_file("PBRForward.vert"), FileUtils::get_shader_path("PBRForward.vert"));
    pci.add_glsl(FileUtils::read_shader_file("PBRForward.frag"), FileUtils::get_shader_path("PBRForward.frag"));
    pci.define("CUBEMAP_PIPELINE", "");
    pci.define("BLEND_MODE", "");
    allocator.get_context().create_named_pipeline("pbr_transparency_cubemap_pipeline", pci);
  );
#endif
  ADD_TASK_TO_PIPE(
    &allocator,
    vuk::PipelineBaseCreateInfo pci;
    pci.add_hlsl(FileUtils::read_shader_file("PostProcess/SSR.hlsl"), FileUtils::get_shader_path("PostProcess/SSR.hlsl"), vuk::HlslShaderStage::eCompute);
    allocator.get_context().create_named_pipeline("ssr_pipeline", pci);
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
    &allocator,
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileUtils::read_shader_file("Debug/Grid.vert"), FileUtils::get_shader_path("Debug/Grid.vert"));
    pci.add_glsl(FileUtils::read_shader_file("Debug/Grid.frag"), FileUtils::get_shader_path("Debug/Grid.frag"));
    allocator.get_context().create_named_pipeline("grid_pipeline", pci);
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
    &allocator,
    vuk::PipelineBaseCreateInfo pci;
    pci.add_hlsl(FileUtils::read_shader_file("Atmosphere/MultiScatterLUT.hlsl"), FileUtils::get_shader_path("Atmosphere/MultiScatterLUT.hlsl"), vuk::HlslShaderStage::eCompute);
    allocator.get_context().create_named_pipeline("sky_multiscatter_pipeline", pci);
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

  wait_for_futures(allocator);

  TaskScheduler::wait_for_all();
}

void DefaultRenderPipeline::commit_descriptor_sets(vuk::Allocator& allocator) {
  OX_SCOPED_ZONE;
  auto& ctx = allocator.get_context();

  scene_data.num_lights = (int)scene_lights.size();
  scene_data.cascade_splits = direct_shadow_ub.cascade_splits;
  scene_data.scissor_normalized = direct_shadow_ub.scissor_normalized;
  scene_data.screen_size = IVec2(Renderer::get_viewport_width(), Renderer::get_viewport_height());
  scene_data.indices.cube_map_index = CUBE_MAP_INDEX;
  scene_data.indices.prefiltered_cube_map_index = PREFILTERED_CUBE_MAP_INDEX;
  scene_data.indices.irradiance_map_index = IRRADIANCE_MAP_INDEX;
  scene_data.indices.brdflut_index = BRDFLUT_INDEX;
  scene_data.indices.sky_transmittance_lut_index = SKY_TRANSMITTANCE_LUT_INDEX;
  scene_data.indices.sky_multiscatter_lut_index = SKY_MULTISCATTER_LUT_INDEX;
  scene_data.indices.shadow_array_index = SHADOW_ARRAY_INDEX;
  scene_data.indices.gtao_index = GTAO_INDEX;

  memcpy(scene_data.cascade_view_projections, direct_shadow_ub.cascade_view_proj_mat, sizeof(Mat4) * 4);

  auto [scene_buff, scene_buff_fut] = create_cpu_buffer(allocator, std::span(&scene_data, 1));
  const auto& scene_buffer = *scene_buff;

  CameraData camera_data = {
    .position = Vec4(current_camera->get_position(), 0.0f),
    .projection_matrix = current_camera->get_projection_matrix(),
    .view_matrix = current_camera->get_view_matrix(),
    .inv_projection_view_matrix = inverse(current_camera->get_projection_matrix() * current_camera->get_view_matrix())
  };

  auto [camera_buff, camera_buff_fut] = create_cpu_buffer(allocator, std::span(&camera_data, 1));
  const auto& camera_buffer = *camera_buff;

  // fill it if it's empty. Or we could allow vulkan to bind null buffers with VK_EXT_robustness2 nullDescriptor feature. 
  if (scene_lights.empty())
    scene_lights.emplace_back(LightData{Vec4{0}, Vec4{0}, Vec4{0}});

  auto [lights_buff, lights_buff_fut] = create_cpu_buffer(allocator, std::span(scene_lights));
  const auto& lights_buffer = *lights_buff;

  scene_lights.clear();

  descriptor_set_00->update_uniform_buffer(0, 0, scene_buffer);
  descriptor_set_00->update_uniform_buffer(1, 0, camera_buffer);
  descriptor_set_00->update_storage_buffer(2, 0, lights_buffer);

  // TODO: A better ID based system for material textures rather than this.
  // since multiple materials might have same textures

  std::vector<Material::Parameters> material_parameters = {};
  uint32_t material_count = 0;
  for (auto& mesh : mesh_draw_list) {
    auto materials = mesh.mesh_base->get_materials(mesh.node_index);
    for (auto& mat : materials) {
      material_parameters.emplace_back(mat->parameters);

      descriptor_set_00->update_sampled_image(8, material_count * Material::TOTAL_PBR_MATERIAL_TEXTURE_COUNT + Material::ALBEDO_MAP_INDEX, *mat->albedo_texture->get_texture().view, vuk::ImageLayout::eReadOnlyOptimalKHR);
      descriptor_set_00->update_sampled_image(8, material_count * Material::TOTAL_PBR_MATERIAL_TEXTURE_COUNT + Material::NORMAL_MAP_INDEX, *mat->normal_texture->get_texture().view, vuk::ImageLayout::eReadOnlyOptimalKHR);
      descriptor_set_00->update_sampled_image(8, material_count * Material::TOTAL_PBR_MATERIAL_TEXTURE_COUNT + Material::PHYSICAL_MAP_INDEX, *mat->metallic_roughness_texture->get_texture().view, vuk::ImageLayout::eReadOnlyOptimalKHR);
      descriptor_set_00->update_sampled_image(8, material_count * Material::TOTAL_PBR_MATERIAL_TEXTURE_COUNT + Material::AO_MAP_INDEX, *mat->ao_texture->get_texture().view, vuk::ImageLayout::eReadOnlyOptimalKHR);
      descriptor_set_00->update_sampled_image(8, material_count * Material::TOTAL_PBR_MATERIAL_TEXTURE_COUNT + Material::EMISSIVE_MAP_INDEX, *mat->emissive_texture->get_texture().view, vuk::ImageLayout::eReadOnlyOptimalKHR);

      material_count += 1;
    }
  }

  if (material_parameters.empty())
    material_parameters.emplace_back(Material::Parameters{});

  auto [matBuff, matBufferFut] = create_cpu_buffer(allocator, std::span(material_parameters));
  auto& mat_buffer = *matBuff;

  descriptor_set_00->update_storage_buffer(3, 0, mat_buffer);

  // scene textures
  if (cube_map) {
    descriptor_set_00->update_sampled_image(4, BRDFLUT_INDEX, *brdf_texture.view, vuk::ImageLayout::eReadOnlyOptimalKHR);
  }
  descriptor_set_00->update_sampled_image(4, SKY_TRANSMITTANCE_LUT_INDEX, *sky_transmittance_lut.view, vuk::ImageLayout::eReadOnlyOptimalKHR);
  descriptor_set_00->update_sampled_image(4, SKY_MULTISCATTER_LUT_INDEX, *sky_multiscatter_lut.view, vuk::ImageLayout::eReadOnlyOptimalKHR);

  // scene uint texture array
  descriptor_set_00->update_sampled_image(5, GTAO_INDEX, *gtao_final_image.view, vuk::ImageLayout::eReadOnlyOptimalKHR);

  // scene cubemap texture array
  if (cube_map) {
    descriptor_set_00->update_sampled_image(6, CUBE_MAP_INDEX, *cube_map->get_texture().view, vuk::ImageLayout::eReadOnlyOptimalKHR);
    descriptor_set_00->update_sampled_image(6, PREFILTERED_CUBE_MAP_INDEX, *prefiltered_texture.view, vuk::ImageLayout::eReadOnlyOptimalKHR);
    descriptor_set_00->update_sampled_image(6, IRRADIANCE_MAP_INDEX, *irradiance_texture.view, vuk::ImageLayout::eReadOnlyOptimalKHR);
  }

  // scene array texture array
  descriptor_set_00->update_sampled_image(7, SHADOW_ARRAY_INDEX, *sun_shadow_texture.view, vuk::ImageLayout::eReadOnlyOptimalKHR);

  // scene Read/Write textures
  descriptor_set_00->update_storage_image(9, SKY_TRANSMITTANCE_LUT_INDEX, *sky_transmittance_lut.view);

  descriptor_set_00->commit(ctx);
}

void DefaultRenderPipeline::create_images(vuk::Allocator& allocator) {
  constexpr auto transmittance_lut_size = vuk::Extent3D{256, 64, 1};
  sky_transmittance_lut = create_texture(allocator, transmittance_lut_size, vuk::Format::eR16G16B16A16Sfloat, DEFAULT_USAGE_FLAGS | vuk::ImageUsageFlagBits::eStorage);

  constexpr auto multi_scatter_lut_size = vuk::Extent3D{32, 32, 1};
  sky_multiscatter_lut = create_texture(allocator, multi_scatter_lut_size, vuk::Format::eR16G16B16A16Sfloat, DEFAULT_USAGE_FLAGS | vuk::ImageUsageFlagBits::eStorage);

  // TODO: extent is static
  gtao_final_image = create_texture(allocator, vuk::Extent3D{1600, 900, 1}, vuk::Format::eR8Uint, DEFAULT_USAGE_FLAGS | vuk::ImageUsageFlagBits::eStorage);

  const auto shadow_size = vuk::Extent3D{(unsigned)RendererCVar::cvar_shadows_size.get(), (unsigned)RendererCVar::cvar_shadows_size.get(), 1};
  sun_shadow_texture = create_texture(allocator, shadow_size, vuk::Format::eD32Sfloat, DEFAULT_USAGE_FLAGS | vuk::ImageUsageFlagBits::eDepthStencilAttachment, false, 4);
}

void DefaultRenderPipeline::create_descriptor_sets(vuk::Allocator& allocator, vuk::Context& ctx) {
  descriptor_set_00 = ctx.create_persistent_descriptorset(allocator, *ctx.get_named_pipeline("pbr_pipeline"), 0, 64);
  descriptor_set_01 = ctx.create_persistent_descriptorset(allocator, *ctx.get_named_pipeline("pbr_pipeline"), 1, 4);

  const vuk::Sampler linear_sampler_clamped = ctx.acquire_sampler(vuk::LinearSamplerClamped, ctx.get_frame_count());
  const vuk::Sampler linear_sampler_repeated = ctx.acquire_sampler(vuk::LinearSamplerRepeated, ctx.get_frame_count());
  const vuk::Sampler nearest_sampler_clamped = ctx.acquire_sampler(vuk::NearestSamplerClamped, ctx.get_frame_count());
  const vuk::Sampler nearest_sampler_repeated = ctx.acquire_sampler(vuk::NearestSamplerRepeated, ctx.get_frame_count());
  descriptor_set_01->update_sampler(0, 0, linear_sampler_clamped);
  descriptor_set_01->update_sampler(0, 1, linear_sampler_repeated);
  descriptor_set_01->update_sampler(0, 2, nearest_sampler_clamped);
  descriptor_set_01->update_sampler(0, 3, nearest_sampler_repeated);

  descriptor_set_01->commit(ctx);
}

void DefaultRenderPipeline::on_dispatcher_events(EventDispatcher& dispatcher) {
  dispatcher.sink<SkyboxLoadEvent>().connect<&DefaultRenderPipeline::update_skybox>(*this);
  dispatcher.sink<ProbeChangeEvent>().connect<&DefaultRenderPipeline::update_parameters>(*this);
}

void DefaultRenderPipeline::on_register_render_object(const MeshComponent& render_object) {
  OX_SCOPED_ZONE;
  mesh_draw_list.emplace_back(render_object);
}

void DefaultRenderPipeline::on_register_light(const TransformComponent& transform, const LightComponent& light) {
  OX_SCOPED_ZONE;
  scene_lights.emplace_back(LightData{
    .position_intensity = {transform.position, light.intensity},
    .color_radius = {light.color, light.range},
    .rotation_type = {transform.rotation, (int)light.type}
  });
  light_buffer_dispatcher.trigger(LightChangeEvent{});
}

void DefaultRenderPipeline::on_register_camera(Camera* camera) {
  OX_SCOPED_ZONE;
  current_camera = camera;
}

void DefaultRenderPipeline::shutdown() { }

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

Scope<vuk::Future> DefaultRenderPipeline::on_render(vuk::Allocator& frame_allocator, const vuk::Future& target, vuk::Dimension3D dim) {
  OX_SCOPED_ZONE;
  if (!current_camera) {
    OX_CORE_ERROR("No camera is set for rendering!");
    // set a temporary one
    if (!default_camera)
      default_camera = create_ref<Camera>();
    current_camera = default_camera.get();
  }

  is_cube_map_pipeline = cube_map == nullptr ? false : true;

  auto vk_context = VulkanContext::get();

  {
    OX_SCOPED_ZONE_N("Frustum culling");
    // frustum cull 
    std::erase_if(
      mesh_draw_list,
      [this](const MeshComponent& mesh_component) {
        const auto camera_frustum = current_camera->get_frustum();
        const auto* node = mesh_component.mesh_base->linear_nodes[mesh_component.node_index];
        const auto aabb = node->aabb.get_transformed(mesh_component.transform);
        if (!aabb.is_on_frustum(camera_frustum)) {
          Renderer::get_stats().drawcall_culled_count += mesh_component.mesh_base->total_primitive_count;
          return true;
        }
        return false;
      });
  }

  std::unordered_map<uint32_t, uint32_t> material_map; //mesh, material

  for (uint32_t mesh_index = 0; mesh_index < mesh_draw_list.size(); mesh_index++) {
    material_map.emplace(mesh_index, mesh_draw_list[mesh_index].mesh_base->get_material_count());
  }

  cumulated_material_map = cumulate_material_map(material_map);

  LightData* dir_light_data = nullptr;

  Vec3 sun_direction = {};
  Vec3 sun_color = {};

  for (auto& light : scene_lights)
    if ((uint32_t)light.rotation_type.w == (uint32_t)(LightComponent::LightType::Directional))
      dir_light_data = &light;

  if (dir_light_data) {
    sun_direction = normalize(glm::vec3(cos(dir_light_data->rotation_type.x)
                                        * cos(dir_light_data->rotation_type.y),
                                        sin(dir_light_data->rotation_type.y),
                                        sin(dir_light_data->rotation_type.x) * cos(dir_light_data->rotation_type.y)));
    sun_color = Vec3(dir_light_data->color_radius) * dir_light_data->position_intensity.w;
  }

  scene_data.sun_direction = sun_direction;
  scene_data.sun_color = Vec4(sun_color, 1.0f);

  if (dir_light_data)
    DirectShadowPass::update_cascades(sun_direction, current_camera, &direct_shadow_ub);

  commit_descriptor_sets(frame_allocator);

  const auto rg = create_ref<vuk::RenderGraph>("DefaultRenderPipelineRenderGraph");

  ubo_vs.view = current_camera->get_view_matrix();
  ubo_vs.cam_pos = current_camera->get_position();
  ubo_vs.projection = current_camera->get_projection_matrix();
  auto [vs_buff, vs_buffer_fut] = create_cpu_buffer(frame_allocator, std::span(&ubo_vs, 1));
  auto& vs_buffer = *vs_buff;

  rg->attach_and_clear_image("black_image", {.format = vk_context->swapchain->format, .sample_count = vuk::SampleCountFlagBits::e1}, vuk::Black<float>);
  rg->inference_rule("black_image", vuk::same_shape_as("final_image"));

  rg->attach_and_clear_image("black_image_uint", {.format = vuk::Format::eR8Uint, .sample_count = vuk::SampleCountFlagBits::e1}, vuk::Black<float>);
  rg->inference_rule("black_image_uint", vuk::same_shape_as("final_image"));

  rg->attach_and_clear_image("normal_image", {.format = vuk::Format::eR16G16B16A16Sfloat, .sample_count = vuk::SampleCountFlagBits::e1}, vuk::Black<float>);
  rg->attach_and_clear_image("depth_image", {.format = vuk::Format::eD32Sfloat, .sample_count = vuk::SampleCountFlagBits::e1}, vuk::DepthOne);
  rg->inference_rule("normal_image", vuk::same_shape_as("final_image"));
  rg->inference_rule("depth_image", vuk::same_shape_as("final_image"));

  depth_pre_pass(rg);

  if (RendererCVar::cvar_gtao_enable.get())
    gtao_pass(frame_allocator, rg);

  if (dir_light_data) {
    auto attachment = vuk::ImageAttachment::from_texture(sun_shadow_texture);
    attachment.view_type = vuk::ImageViewType::e2DArray;
    rg->attach_and_clear_image("shadow_map_array", attachment, vuk::DepthOne);
    cascaded_shadow_pass(rg);
  }
  else {
    rg->attach_and_clear_image("shadow_array_output", vuk::ImageAttachment::from_texture(sun_shadow_texture), vuk::DepthOne);
  }

  sky_transmittance_pass(rg);
  sky_multiscatter_pass(rg);

  if (!is_cube_map_pipeline)
    sky_view_lut_pass(rg);

  geometry_pass(rg);

  rg->attach_and_clear_image("pbr_image", {.format = vuk::Format::eR32G32B32A32Sfloat, .sample_count = vuk::SampleCountFlagBits::e1}, vuk::Black<float>);
  rg->inference_rule("pbr_image", vuk::same_shape_as("final_image"));

  if (RendererCVar::cvar_ssr_enable.get())
    ssr_pass(frame_allocator, rg, vs_buffer);

  if (RendererCVar::cvar_bloom_enable.get())
    bloom_pass(rg);

  scene_data.post_processing_data.tonemapper = RendererCVar::cvar_tonemapper.get();
  scene_data.post_processing_data.exposure = RendererCVar::cvar_exposure.get();
  scene_data.post_processing_data.gamma = RendererCVar::cvar_gamma.get();
  scene_data.post_processing_data.enable_bloom = RendererCVar::cvar_bloom_enable.get();
  scene_data.post_processing_data.enable_ssr = RendererCVar::cvar_ssr_enable.get();
  scene_data.post_processing_data.enable_gtao = RendererCVar::cvar_gtao_enable.get();

  auto [final_buff, final_buff_fut] = create_cpu_buffer(frame_allocator, std::span(&scene_data.post_processing_data, 1));
  auto& final_buffer = *final_buff;

  auto [ssr_resouce, ssr_name] = get_attachment_or_black("ssr_output", RendererCVar::cvar_ssr_enable.get());
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
    apply_grid(rg.get(), pbr_image_name, "depth_output", frame_allocator);
    pbr_image_name = pbr_image_name.append("+");
  }

  std::vector final_resources = {
    "final_image"_image >> vuk::eColorRW >> "final_output",
    vuk::Resource{pbr_image_name, vuk::Resource::Type::eImage, vuk::eFragmentSampled},
    ssr_resouce,
    bloom_resource,
  };

  rg->attach_in("final_image", target);

  rg->add_pass({
    .name = "final_pass",
    .resources = std::move(final_resources),
    .execute = [this, final_buffer, ssr_name, bloom_name, pbr_image_name](vuk::CommandBuffer& command_buffer) {
      this->mesh_draw_list.clear();

      command_buffer.bind_graphics_pipeline("final_pipeline")
                    .set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .broadcast_color_blend(vuk::BlendPreset::eOff)
                    .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
                    .bind_sampler(0, 0, vuk::LinearSamplerClamped)
                    .bind_image(0, 0, pbr_image_name)
                    .bind_sampler(0, 3, vuk::LinearSamplerClamped)
                    .bind_image(0, 3, ssr_name)
                    .bind_sampler(0, 4, {})
                    .bind_image(0, 4, bloom_name)
                    .bind_buffer(0, 5, final_buffer)
                    .draw(3, 1, 0, 0);
    }
  });

  return create_scope<vuk::Future>(rg, "final_output");
}

void DefaultRenderPipeline::sky_transmittance_pass(const Ref<vuk::RenderGraph>& rg) {
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

void DefaultRenderPipeline::sky_multiscatter_pass(const Ref<vuk::RenderGraph>& rg) {
  OX_SCOPED_ZONE;

  IVec2 lut_size = {32, 32};

  rg->attach_and_clear_image("sky_multiscatter_lut", vuk::ImageAttachment::from_texture(sky_multiscatter_lut), vuk::Black<float>);

  rg->add_pass({
    .name = "sky_multiscatter_lut_pass",
    .resources = {
      "sky_multiscatter_lut"_image >> vuk::eComputeRW,
      "sky_transmittance_lut+"_image >> vuk::eComputeSampled,
    },
    .execute = [lut_size](vuk::CommandBuffer& command_buffer) {
      command_buffer.bind_compute_pipeline("sky_multiscatter_pipeline")
                    .bind_image(0, 0, "sky_transmittance_lut+")
                    .bind_image(0, 1, "sky_multiscatter_lut")
                    .bind_sampler(0, 2, vuk::LinearSamplerClamped)
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

void DefaultRenderPipeline::depth_pre_pass(const Ref<vuk::RenderGraph>& rg) {
  OX_SCOPED_ZONE;
  rg->add_pass({
    .name = "depth_pre_pass",
    .resources = {
      "normal_image"_image >> vuk::eColorRW >> "normal_output",
      "depth_image"_image >> vuk::eDepthStencilRW >> "depth_output"
    },
    .execute = [this](vuk::CommandBuffer& command_buffer) {
      command_buffer.bind_persistent(0, *descriptor_set_00)
                    .bind_persistent(1, *descriptor_set_01);

      command_buffer.set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
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

      for (uint32_t i = 0; i < mesh_draw_list.size(); i++) {
        const auto& mesh = mesh_draw_list[i];
        mesh.mesh_base->bind_index_buffer(command_buffer);

        const auto node = mesh.mesh_base->linear_nodes[mesh.node_index];
        for (const auto primitive : node->mesh_data->primitives) {
          if (primitive->material->parameters.alpha_mode == (uint32_t)Material::AlphaMode::Mask
              || primitive->material->parameters.alpha_mode == (uint32_t)Material::AlphaMode::Blend) {
            continue;
          }

          const auto material_index = get_material_index(i, primitive->material_index);
          const auto pc = ShaderPC{mesh.transform, mesh.mesh_base->vertex_buffer->device_address, material_index};
          command_buffer.push_constants(vuk::ShaderStageFlagBits::eVertex | vuk::ShaderStageFlagBits::eFragment, 0, pc);

          Renderer::draw_indexed(command_buffer, primitive->index_count, 1, primitive->first_index, 0, 0);
        }
      }
    }
  });
}

void DefaultRenderPipeline::geometry_pass(const Ref<vuk::RenderGraph>& rg) {
  OX_SCOPED_ZONE;

  auto [gtao_resource, gtao_name] = get_attachment_or_black_uint("gtao_final_output", RendererCVar::cvar_gtao_enable.get());

  const auto resources = std::vector<vuk::Resource>{
    "pbr_image"_image >> vuk::eColorRW >> "pbr_output",
    "depth_output"_image >> vuk::eDepthStencilRead,
    "shadow_array_output"_image >> vuk::eFragmentSampled,
    "sky_transmittance_lut+"_image >> vuk::eFragmentSampled,
    "sky_multiscatter_lut+"_image >> vuk::eFragmentSampled,
    gtao_resource
  };

  rg->add_pass({
    .name = "geometry_pass",
    .resources = resources,
    .execute = [this](vuk::CommandBuffer& command_buffer) {
      command_buffer.bind_persistent(0, *descriptor_set_00)
                    .bind_persistent(1, *descriptor_set_01);
      if (is_cube_map_pipeline) {
        auto view = current_camera->get_view_matrix();
        view[3] = Vec4(0, 0, 0, 1);
        const struct SkyboxPushConstant {
          Mat4 view;
        } skybox_push_constant = {view};

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
                      .push_constants(vuk::ShaderStageFlagBits::eVertex, 0, skybox_push_constant);

        m_cube->bind_index_buffer(command_buffer)
              ->bind_vertex_buffer(command_buffer);
        command_buffer.draw_indexed(m_cube->index_count, 1, 0, 0, 0);
      }
      else {
        command_buffer.set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
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
      }

      if (is_cube_map_pipeline) {
        command_buffer.bind_graphics_pipeline("pbr_cubemap_pipeline");
      }
      else {
        command_buffer.bind_graphics_pipeline("pbr_pipeline");
      }

      command_buffer.bind_persistent(0, *descriptor_set_00)
                    .bind_persistent(1, *descriptor_set_01);

      command_buffer.set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
                    .set_rasterization({.cullMode = vuk::CullModeFlagBits::eBack}).
                     set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .broadcast_color_blend({})
                    .set_depth_stencil(vuk::PipelineDepthStencilStateCreateInfo{
                       .depthTestEnable = true,
                       .depthWriteEnable = false,
                       .depthCompareOp = vuk::CompareOp::eLessOrEqual,
                     });

      for (uint32_t i = 0; i < mesh_draw_list.size(); i++) {
        auto& mesh = mesh_draw_list[i];

        mesh.mesh_base->bind_index_buffer(command_buffer);

        std::vector<Mesh::Primitive*> transparent_primitives = {};

        const auto node = mesh.mesh_base->linear_nodes[mesh.node_index];
        for (auto primitive : node->mesh_data->primitives) {
          if (primitive->material->parameters.alpha_mode == (uint32_t)Material::AlphaMode::Blend) {
            transparent_primitives.emplace_back(primitive);
            break;
          }

          const auto material_index = get_material_index(i, primitive->material_index);

          const auto pc = ShaderPC{mesh.transform, mesh.mesh_base->vertex_buffer->device_address, material_index};

          command_buffer.push_constants(vuk::ShaderStageFlagBits::eVertex | vuk::ShaderStageFlagBits::eFragment, 0, pc);

          Renderer::draw_indexed(command_buffer, primitive->index_count, 1, primitive->first_index, 0, 0);
        }
#if 0
        // Transparency pass
        if (is_cube_map_pipeline) {
          command_buffer.bind_graphics_pipeline("pbr_transparency_cubemap_pipeline");
        }
        else {
          command_buffer.bind_graphics_pipeline("pbr_transparency_pipeline");
        }

        command_buffer.broadcast_color_blend(vuk::BlendPreset::eAlphaBlend);

        for (const auto& primitive : transparent_primitives) {
          const auto material_index = get_material_index(material_map, i, primitive->material_index);

          command_buffer.push_constants(vuk::ShaderStageFlagBits::eVertex | vuk::ShaderStageFlagBits::eFragment, 0, mesh.transform)
                        .push_constants(vuk::ShaderStageFlagBits::eFragment, sizeof(glm::mat4), material_index);

          primitive->material->bind_textures(command_buffer);

          Renderer::draw_indexed(command_buffer, primitive->index_count, 1, primitive->first_index, 0, 0);
        }
#endif
      }
    }
  });
}

void DefaultRenderPipeline::bloom_pass(const Ref<vuk::RenderGraph>& rg) {
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

void DefaultRenderPipeline::gtao_pass(vuk::Allocator& frame_allocator, const Ref<vuk::RenderGraph>& rg) {
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

  rg->attach_and_clear_image("gtao_final_image", vuk::ImageAttachment::from_texture(gtao_final_image), vuk::Black<float>);
  rg->inference_rule("gtao_final_image", vuk::same_extent_as("final_image"));
}

void DefaultRenderPipeline::ssr_pass(vuk::Allocator& frame_allocator, const Ref<vuk::RenderGraph>& rg, vuk::Buffer& vs_buffer) {
  OX_SCOPED_ZONE;
  struct SSRData {
    int samples = 64;
    int binary_search_samples = 16;
    float max_dist = 100.0f;
  } ssr_data;

  auto [ssr_buff, ssr_buff_fut] = create_cpu_buffer(frame_allocator, std::span(&ssr_data, 1));
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
                    .bind_image(0, 0, "ssr_image")
                    .bind_image(0, 1, "pbr_output")
                    .bind_image(0, 2, "depth_output")
                    .bind_image(0, 3, "normal_output")
                    .bind_buffer(0, 5, vs_buffer)
                    .bind_buffer(0, 6, ssr_buffer)
                    .bind_sampler(0, 7, vuk::LinearSamplerClamped)
                    .dispatch((Renderer::get_viewport_width() + 8 - 1) / 8, (Renderer::get_viewport_height() + 8 - 1) / 8, 1);
    }
  });

  rg->attach_and_clear_image("ssr_image", {.format = vuk::Format::eR32G32B32A32Sfloat, .sample_count = vuk::SampleCountFlagBits::e1}, vuk::Black<float>);
  rg->inference_rule("ssr_image", vuk::same_shape_as("final_image"));
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

void DefaultRenderPipeline::apply_grid(vuk::RenderGraph* rg, const vuk::Name dst, const vuk::Name depth_image, vuk::Allocator& frame_allocator) const {
  OX_SCOPED_ZONE;
  struct GridVertexBuffer {
    Mat4 view;
    Mat4 proj;
  } grid_vertex_data;

  grid_vertex_data.view = current_camera->get_view_matrix();
  grid_vertex_data.proj = current_camera->get_projection_matrix();

  auto [vgrid_buff, vgrid_buff_fut] = create_cpu_buffer(frame_allocator, std::span(&grid_vertex_data, 1));
  auto& grid_vertex_buffer = *vgrid_buff;

  struct GridFragmentBuffer {
    Vec4 camera_pos;
    float near;
    float far;
    float max_distance;
  } grid_fragment_data;

  grid_fragment_data.camera_pos = Vec4(current_camera->get_position(), 0.0);
  grid_fragment_data.near = current_camera->get_near();
  grid_fragment_data.far = current_camera->get_far();
  grid_fragment_data.max_distance = RendererCVar::cvar_draw_grid_distance.get();

  auto [fgrid_buff, fgrid_buff_fut] = create_cpu_buffer(frame_allocator, std::span(&grid_fragment_data, 1));
  auto& grid_fragment_buffer = *fgrid_buff;

  rg->add_pass({
    .name = "grid",
    .resources = {
      vuk::Resource(dst, vuk::Resource::Type::eImage, vuk::eColorWrite, dst.append("+")),
      vuk::Resource(depth_image, vuk::Resource::Type::eImage, vuk::eDepthStencilRead)
    },
    .execute = [this, grid_vertex_buffer, grid_fragment_buffer](vuk::CommandBuffer& command_buffer) {
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
                    .bind_buffer(0, 0, grid_vertex_buffer)
                    .bind_buffer(0, 1, grid_fragment_buffer);

      m_quad->bind_index_buffer(command_buffer)
            ->bind_vertex_buffer(command_buffer);

      Renderer::draw_indexed(command_buffer, m_quad->index_count, 1, 0, 0, 0);
    }
  });
}

void DefaultRenderPipeline::cascaded_shadow_pass(const Ref<vuk::RenderGraph>& rg) {
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
                      .bind_persistent(1, *descriptor_set_01);

        command_buffer.bind_graphics_pipeline("shadow_pipeline")
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

        for (const auto& mesh : mesh_draw_list) {
          mesh.mesh_base->bind_index_buffer(command_buffer);

          const auto node = mesh.mesh_base->linear_nodes[mesh.node_index];
          for (const auto primitive : node->mesh_data->primitives) {
            const struct PushConst {
              Mat4 model;
              uint64_t vertex_buffer_btr;
              uint32_t cascade_index = 0;
            } push_const = {mesh.transform, mesh.mesh_base->vertex_buffer->device_address, cascade_index};

            command_buffer.push_constants(vuk::ShaderStageFlagBits::eVertex, 0, push_const);

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

void DefaultRenderPipeline::sky_view_lut_pass(const Ref<vuk::RenderGraph>& rg) {
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
                    .bind_persistent(1, *descriptor_set_01)
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

void DefaultRenderPipeline::debug_pass(const Ref<vuk::RenderGraph>& rg, const vuk::Name dst, const char* depth, vuk::Allocator& frame_allocator) const {
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

  // fill in if empty
  // TODO(hatrickek): (temporary solution until we move the vertex buffer initialization to DebugRenderer::init())
  if (vertices.empty())
    vertices.emplace_back(Vertex{});

  auto [v_buff, v_buff_fut] = create_cpu_buffer(frame_allocator, std::span(vertices));
  auto& vertex_buffer = *v_buff;

  auto& index_buffer = *DebugRenderer::get_instance()->get_global_index_buffer();

  // not depth tested
  rg->add_pass({
    .name = "debug_shapes_pass",
    .resources = {
      vuk::Resource(dst, vuk::Resource::Type::eImage, vuk::eColorWrite, dst.append("+")),
    },
    .execute = [this, buffer, index_count, vertex_buffer, index_buffer](vuk::CommandBuffer& command_buffer) {
      const auto vertex_layout = vuk::Packed{
        vuk::Format::eR32G32B32Sfloat,
        vuk::Format::eR32G32B32Sfloat,
        vuk::Ignore{sizeof(Vertex) - (sizeof(Vertex::position) + sizeof(Vertex::normal))}
      };

      command_buffer.bind_graphics_pipeline("unlit_pipeline")
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
    }
  });

  // TODO: depth tested 

  DebugRenderer::reset(true);
}
}
