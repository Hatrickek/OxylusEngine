#include "DefaultRenderPipeline.h"

#include <ankerl/unordered_dense.h>
#include <cstdint>
#include <glm/gtc/type_ptr.inl>
#include <vuk/Partials.hpp>

#include "DebugRenderer.hpp"
#include "RendererCommon.h"
#include "SceneRendererEvents.h"

#include "Core/App.hpp"
#include "Passes/Prefilter.hpp"

#include "Scene/Scene.hpp"

#include "Thread/TaskScheduler.hpp"

#include "Utils/CVars.hpp"
#include "Utils/Log.hpp"
#include "Utils/Profiler.hpp"
#include "Utils/Timer.hpp"

#include "Vulkan/VkContext.hpp"

#include "Renderer.hpp"
#include "Utils/SPD.hpp"
#include "Utils/VukCommon.hpp"

#include "Utils/RectPacker.hpp"

namespace ox {
VkDescriptorSetLayoutBinding bindless_binding(uint32_t binding, vuk::DescriptorType descriptor_type, uint32_t count = 1024) {
  return {
    .binding = binding,
    .descriptorType = (VkDescriptorType)descriptor_type,
    .descriptorCount = count,
    .stageFlags = (VkShaderStageFlags)vuk::ShaderStageFlagBits::eAll,
  };
}

void DefaultRenderPipeline::init(vuk::Allocator& allocator) {
  OX_SCOPED_ZONE;

  const Timer timer = {};

  load_pipelines(allocator);

  if (initalized)
    return;

  const auto task_scheduler = App::get_system<TaskScheduler>();

  task_scheduler->add_task([this] { this->m_quad = RendererCommon::generate_quad(); });
  task_scheduler->add_task([this] { this->m_cube = RendererCommon::generate_cube(); });
  task_scheduler->add_task([this, &allocator] { create_static_resources(allocator); });
  task_scheduler->add_task([this, &allocator] { create_descriptor_sets(allocator); });

  task_scheduler->wait_for_all();

  initalized = true;

  OX_LOG_INFO("DefaultRenderPipeline initialized: {} ms", timer.get_elapsed_ms());
}

void DefaultRenderPipeline::load_pipelines(vuk::Allocator& allocator) {
  OX_SCOPED_ZONE;

  vuk::PipelineBaseCreateInfo bindless_pci = {};
  vuk::DescriptorSetLayoutCreateInfo bindless_dslci_00 = {};
  bindless_dslci_00.bindings = {
    bindless_binding(0, vuk::DescriptorType::eUniformBuffer, 1),
    bindless_binding(1, vuk::DescriptorType::eStorageBuffer),
    bindless_binding(2, vuk::DescriptorType::eSampledImage),
    bindless_binding(3, vuk::DescriptorType::eSampledImage),
    bindless_binding(4, vuk::DescriptorType::eSampledImage, 8),
    bindless_binding(5, vuk::DescriptorType::eSampledImage, 8),
    bindless_binding(6, vuk::DescriptorType::eStorageImage),
    bindless_binding(7, vuk::DescriptorType::eSampledImage),
    bindless_binding(8, vuk::DescriptorType::eSampler, 5),
    bindless_binding(9, vuk::DescriptorType::eSampler, 5),
  };
  bindless_dslci_00.index = 0;
  for (int i = 0; i < 10; i++)
    bindless_dslci_00.flags.emplace_back(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
  bindless_pci.explicit_set_layouts.emplace_back(bindless_dslci_00);

  using SS = vuk::HlslShaderStage;

#define SHADER_FILE(path) FileSystem::read_shader_file(path), FileSystem::get_shader_path(path)

  auto* task_scheduler = App::get_system<TaskScheduler>();

  task_scheduler->add_task([=]() mutable {
    bindless_pci.add_hlsl(SHADER_FILE("DepthNormalPrePass.hlsl"), SS::eVertex, "VSmain");
    bindless_pci.add_hlsl(SHADER_FILE("DepthNormalPrePass.hlsl"), SS::ePixel, "PSmain");
    TRY(allocator.get_context().create_named_pipeline("depth_pre_pass_pipeline", bindless_pci))
  });

  task_scheduler->add_task([=]() mutable {
    bindless_pci.add_hlsl(SHADER_FILE("DirectionalShadowPass.hlsl"), SS::eVertex, "VSmain");
    TRY(allocator.get_context().create_named_pipeline("shadow_pipeline", bindless_pci))
  });

  task_scheduler->add_task([=]() mutable {
    bindless_pci.add_hlsl(SHADER_FILE("PBRForward.hlsl"), SS::eVertex, "VSmain");
    bindless_pci.add_hlsl(SHADER_FILE("PBRForward.hlsl"), SS::ePixel, "PSmain");
    TRY(allocator.get_context().create_named_pipeline("pbr_pipeline", bindless_pci))
  });

  task_scheduler->add_task([=]() mutable {
    bindless_pci.add_hlsl(SHADER_FILE("PBRForward.hlsl"), SS::eVertex, "VSmain");
    bindless_pci.add_hlsl(SHADER_FILE("PBRForward.hlsl"), SS::ePixel, "PSmain");
    bindless_pci.define("TRANSPARENT", "");
    TRY(allocator.get_context().create_named_pipeline("pbr_transparency_pipeline", bindless_pci))
  });

  task_scheduler->add_task([&allocator]() mutable {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_hlsl(SHADER_FILE("FullscreenTriangle.hlsl"), SS::eVertex);
    pci.add_glsl(SHADER_FILE("FinalPass.frag"));
    TRY(allocator.get_context().create_named_pipeline("final_pipeline", pci))
  });

  // --- GTAO ---
  task_scheduler->add_task([&allocator]() mutable {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_hlsl(SHADER_FILE("GTAO/GTAO_First.hlsl"), SS::eCompute, "CSPrefilterDepths16x16");
    pci.define("XE_GTAO_FP32_DEPTHS", "");
    pci.define("XE_GTAO_USE_HALF_FLOAT_PRECISION", "0");
    pci.define("XE_GTAO_USE_DEFAULT_CONSTANTS", "0");
    TRY(allocator.get_context().create_named_pipeline("gtao_first_pipeline", pci))
  });

  task_scheduler->add_task([&allocator]() mutable {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_hlsl(SHADER_FILE("GTAO/GTAO_Main.hlsl"), SS::eCompute, "CSGTAOHigh");
    pci.define("XE_GTAO_FP32_DEPTHS", "");
    pci.define("XE_GTAO_USE_HALF_FLOAT_PRECISION", "0");
    pci.define("XE_GTAO_USE_DEFAULT_CONSTANTS", "0");
    TRY(allocator.get_context().create_named_pipeline("gtao_main_pipeline", pci))
  });

  task_scheduler->add_task([&allocator]() mutable {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_hlsl(SHADER_FILE("GTAO/GTAO_Final.hlsl"), SS::eCompute, "CSDenoisePass");
    pci.define("XE_GTAO_FP32_DEPTHS", "");
    pci.define("XE_GTAO_USE_HALF_FLOAT_PRECISION", "0");
    pci.define("XE_GTAO_USE_DEFAULT_CONSTANTS", "0");
    TRY(allocator.get_context().create_named_pipeline("gtao_denoise_pipeline", pci))
  });

  task_scheduler->add_task([&allocator]() mutable {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_hlsl(SHADER_FILE("GTAO/GTAO_Final.hlsl"), SS::eCompute, "CSDenoiseLastPass");
    pci.define("XE_GTAO_FP32_DEPTHS", "");
    pci.define("XE_GTAO_USE_HALF_FLOAT_PRECISION", "0");
    pci.define("XE_GTAO_USE_DEFAULT_CONSTANTS", "0");
    TRY(allocator.get_context().create_named_pipeline("gtao_final_pipeline", pci))
  });

  task_scheduler->add_task([&allocator]() mutable {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_hlsl(SHADER_FILE("FullscreenTriangle.hlsl"), SS::eVertex);
    pci.add_glsl(SHADER_FILE("PostProcess/FXAA.frag"));
    TRY(allocator.get_context().create_named_pipeline("fxaa_pipeline", pci))
  });

  // --- Bloom ---
  task_scheduler->add_task([&allocator]() mutable {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(SHADER_FILE("PostProcess/BloomPrefilter.comp"));
    TRY(allocator.get_context().create_named_pipeline("bloom_prefilter_pipeline", pci))
  });

  task_scheduler->add_task([&allocator]() mutable {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(SHADER_FILE("PostProcess/BloomDownsample.comp"));
    TRY(allocator.get_context().create_named_pipeline("bloom_downsample_pipeline", pci))
  });

  task_scheduler->add_task([&allocator]() mutable {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(SHADER_FILE("PostProcess/BloomUpsample.comp"));
    TRY(allocator.get_context().create_named_pipeline("bloom_upsample_pipeline", pci))
  });

  task_scheduler->add_task([=]() mutable {
    bindless_pci.add_hlsl(SHADER_FILE("Debug/Grid.hlsl"), SS::eVertex);
    bindless_pci.add_hlsl(SHADER_FILE("Debug/Grid.hlsl"), SS::ePixel, "PSmain");
    TRY(allocator.get_context().create_named_pipeline("grid_pipeline", bindless_pci))
  });

  task_scheduler->add_task([&allocator]() mutable {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(SHADER_FILE("Debug/Unlit.vert"));
    pci.add_glsl(SHADER_FILE("Debug/Unlit.frag"));
    TRY(allocator.get_context().create_named_pipeline("unlit_pipeline", pci))
  });

  // --- Atmosphere ---
  task_scheduler->add_task([=]() mutable {
    bindless_pci.add_hlsl(SHADER_FILE("Atmosphere/TransmittanceLUT.hlsl"), SS::eCompute);
    TRY(allocator.get_context().create_named_pipeline("sky_transmittance_pipeline", bindless_pci))
  });

  task_scheduler->add_task([=]() mutable {
    bindless_pci.add_hlsl(SHADER_FILE("Atmosphere/MultiScatterLUT.hlsl"), SS::eCompute);
    TRY(allocator.get_context().create_named_pipeline("sky_multiscatter_pipeline", bindless_pci))
  });

  task_scheduler->add_task([=]() mutable {
    bindless_pci.add_hlsl(SHADER_FILE("FullscreenTriangle.hlsl"), SS::eVertex);
    bindless_pci.add_hlsl(SHADER_FILE("Atmosphere/SkyView.hlsl"), SS::ePixel);
    TRY(allocator.get_context().create_named_pipeline("sky_view_pipeline", bindless_pci))
  });

  task_scheduler->add_task([=]() mutable {
    bindless_pci.add_hlsl(SHADER_FILE("Atmosphere/SkyViewFinal.hlsl"), SS::eVertex, "VSmain");
    bindless_pci.add_hlsl(SHADER_FILE("Atmosphere/SkyViewFinal.hlsl"), SS::ePixel, "PSmain");
    TRY(allocator.get_context().create_named_pipeline("sky_view_final_pipeline", bindless_pci))
  });

  task_scheduler->add_task([=]() mutable {
    bindless_pci.add_hlsl(SHADER_FILE("Atmosphere/SkyEnvMap.hlsl"), SS::eVertex, "VSmain");
    bindless_pci.add_hlsl(SHADER_FILE("Atmosphere/SkyEnvMap.hlsl"), SS::ePixel, "PSmain");
    TRY(allocator.get_context().create_named_pipeline("sky_envmap_pipeline", bindless_pci))
  });

  task_scheduler->wait_for_all();
}

void DefaultRenderPipeline::clear() {
  render_queue.clear();
  mesh_component_list.clear();
  scene_lights.clear();
  light_datas.clear();
  dir_light_data = nullptr;
}

void DefaultRenderPipeline::bind_camera_buffer(vuk::CommandBuffer& command_buffer) {
  const auto cb = command_buffer.scratch_buffer<CameraCB>(1, 0);
  *cb = camera_cb;
}

DefaultRenderPipeline::CameraData DefaultRenderPipeline::get_main_camera_data() const {
  CameraData camera_data{
    .position = Vec4(current_camera->get_position(), 0.0f),
    .projection = current_camera->get_projection_matrix(),
    .inv_projection = current_camera->get_inv_projection_matrix(),
    .view = current_camera->get_view_matrix(),
    .inv_view = current_camera->get_inv_view_matrix(),
    .projection_view = current_camera->get_projection_matrix() * current_camera->get_view_matrix(),
    .inv_projection_view = current_camera->get_inverse_projection_view(),
    .previous_projection = current_camera->get_projection_matrix(),
    .previous_inv_projection = current_camera->get_inv_projection_matrix(),
    .previous_view = current_camera->get_view_matrix(),
    .previous_inv_view = current_camera->get_inv_view_matrix(),
    .previous_projection_view = current_camera->get_projection_matrix() * current_camera->get_view_matrix(),
    .previous_inv_projection_view = current_camera->get_inverse_projection_view(),
    .near_clip = current_camera->get_near(),
    .far_clip = current_camera->get_far(),
    .fov = current_camera->get_fov(),
    .output_index = 0,
  };

  // if (RendererCVar::cvar_fsr_enable.get())
  // current_camera->set_jitter(fsr.get_jitter());

  camera_data.temporalaa_jitter = current_camera->get_jitter();
  camera_data.temporalaa_jitter_prev = current_camera->get_previous_jitter();

  return camera_data;
}

void DefaultRenderPipeline::create_dir_light_cameras(const LightComponent& light,
                                                     Camera& camera,
                                                     std::vector<CameraSH>& camera_data,
                                                     uint32_t cascade_count) {
  OX_SCOPED_ZONE;

  const auto lightRotation = glm::toMat4(glm::quat(light.rotation));
  const auto to = math::transform_normal(Vec4(0.0f, -1.0f, 0.0f, 0.0f), lightRotation);
  const auto up = math::transform_normal(Vec4(0.0f, 0.0f, 1.0f, 0.0f), lightRotation);
  auto light_view = glm::lookAt(Vec3{}, Vec3(to), Vec3(up));

  const auto unproj = camera.get_inverse_projection_view();

  Vec4 frustum_corners[8] = {
    math::transform_coord(Vec4(-1.f, -1.f, 1.f, 1.f), unproj), // near
    math::transform_coord(Vec4(-1.f, -1.f, 0.f, 1.f), unproj), // far
    math::transform_coord(Vec4(-1.f, 1.f, 1.f, 1.f), unproj),  // near
    math::transform_coord(Vec4(-1.f, 1.f, 0.f, 1.f), unproj),  // far
    math::transform_coord(Vec4(1.f, -1.f, 1.f, 1.f), unproj),  // near
    math::transform_coord(Vec4(1.f, -1.f, 0.f, 1.f), unproj),  // far
    math::transform_coord(Vec4(1.f, 1.f, 1.f, 1.f), unproj),   // near
    math::transform_coord(Vec4(1.f, 1.f, 0.f, 1.f), unproj),   // far
  };

  // Compute shadow cameras:
  for (uint32_t cascade = 0; cascade < cascade_count; ++cascade) {
    // Compute cascade bounds in light-view-space from the main frustum corners:
    const float farPlane = camera.get_far();
    const float split_near = cascade == 0 ? 0 : light.cascade_distances[cascade - 1] / farPlane;
    const float split_far = light.cascade_distances[cascade] / farPlane;

    Vec4 corners[8] = {
      math::transform(lerp(frustum_corners[0], frustum_corners[1], split_near), light_view),
      math::transform(lerp(frustum_corners[0], frustum_corners[1], split_far), light_view),
      math::transform(lerp(frustum_corners[2], frustum_corners[3], split_near), light_view),
      math::transform(lerp(frustum_corners[2], frustum_corners[3], split_far), light_view),
      math::transform(lerp(frustum_corners[4], frustum_corners[5], split_near), light_view),
      math::transform(lerp(frustum_corners[4], frustum_corners[5], split_far), light_view),
      math::transform(lerp(frustum_corners[6], frustum_corners[7], split_near), light_view),
      math::transform(lerp(frustum_corners[6], frustum_corners[7], split_far), light_view),
    };

    // Compute cascade bounding sphere center:
    Vec4 center = {};
    for (int j = 0; j < std::size(corners); ++j) {
      center += corners[j];
    }
    center /= float(std::size(corners));

    // Compute cascade bounding sphere radius:
    float radius = 0;
    for (int j = 0; j < std::size(corners); ++j) {
      radius = std::max(radius, glm::length(corners[j] - center));
    }

    // Fit AABB onto bounding sphere:
    auto vRadius = Vec4(radius);
    auto vMin = center - vRadius;
    auto vMax = center + vRadius;

    // Snap cascade to texel grid:
    const auto extent = vMax - vMin;
    const auto texelSize = extent / float(light.shadow_rect.w);
    vMin = glm::floor(vMin / texelSize) * texelSize;
    vMax = glm::floor(vMax / texelSize) * texelSize;
    center = (vMin + vMax) * 0.5f;

    // Extrude bounds to avoid early shadow clipping:
    float ext = abs(center.z - vMin.z);
    ext = std::max(ext, std::min(1500.0f, farPlane) * 0.5f);
    vMin.z = center.z - ext;
    vMax.z = center.z + ext;

    const auto light_projection = glm::ortho(vMin.x, vMax.x, vMin.y, vMax.y, vMax.z, vMin.z); // reversed Z
    const auto view_proj = light_projection * light_view;

    camera_data[cascade].projection_view = view_proj;
    camera_data[cascade].frustum = Frustum::from_matrix(view_proj);
  }
}

void DefaultRenderPipeline::update_frame_data(vuk::Allocator& allocator) {
  OX_SCOPED_ZONE;
  auto& ctx = allocator.get_context();

  scene_data.num_lights = (int)scene_lights.size();
  scene_data.grid_max_distance = RendererCVar::cvar_draw_grid_distance.get();
  scene_data.screen_size = IVec2(Renderer::get_viewport_width(), Renderer::get_viewport_height());
  scene_data.screen_size_rcp = {1.0f / (float)std::max(1u, scene_data.screen_size.x), 1.0f / (float)std::max(1u, scene_data.screen_size.y)};

  scene_data.indices.pbr_image_index = PBR_IMAGE_INDEX;
  scene_data.indices.normal_image_index = NORMAL_IMAGE_INDEX;
  scene_data.indices.depth_image_index = DEPTH_IMAGE_INDEX;
  scene_data.indices.sky_transmittance_lut_index = SKY_TRANSMITTANCE_LUT_INDEX;
  scene_data.indices.sky_multiscatter_lut_index = SKY_MULTISCATTER_LUT_INDEX;
  scene_data.indices.velocity_image_index = VELOCITY_IMAGE_INDEX;
  scene_data.indices.sky_env_map_index = SKY_ENVMAP_INDEX;
  scene_data.indices.shadow_array_index = SHADOW_ATLAS_INDEX;
  scene_data.indices.gtao_buffer_image_index = GTAO_BUFFER_IMAGE_INDEX;
  scene_data.indices.lights_buffer_index = LIGHTS_BUFFER_INDEX;
  scene_data.indices.materials_buffer_index = MATERIALS_BUFFER_INDEX;
  scene_data.indices.mesh_instance_buffer_index = MESH_INSTANCES_BUFFER_INDEX;
  scene_data.indices.entites_buffer_index = ENTITIES_BUFFER_INDEX;

  scene_data.post_processing_data.tonemapper = RendererCVar::cvar_tonemapper.get();
  scene_data.post_processing_data.exposure = RendererCVar::cvar_exposure.get();
  scene_data.post_processing_data.gamma = RendererCVar::cvar_gamma.get();
  scene_data.post_processing_data.enable_bloom = RendererCVar::cvar_bloom_enable.get();
  scene_data.post_processing_data.enable_ssr = RendererCVar::cvar_ssr_enable.get();
  scene_data.post_processing_data.enable_gtao = RendererCVar::cvar_gtao_enable.get();

  auto [scene_buff, scene_buff_fut] = create_cpu_buffer(allocator, std::span(&scene_data, 1));
  const auto& scene_buffer = *scene_buff;

  std::vector<Material::Parameters> material_parameters = {};
  for (auto& mesh : mesh_component_list) {
    auto& materials = mesh.materials;
    for (auto& mat : materials) {
      material_parameters.emplace_back(mat->parameters);

      const auto& albedo = mat->get_albedo_texture();
      const auto& normal = mat->get_normal_texture();
      const auto& physical = mat->get_physical_texture();
      const auto& ao = mat->get_ao_texture();
      const auto& emissive = mat->get_emissive_texture();

      if (albedo && albedo->is_valid_id())
        descriptor_set_00->update_sampled_image(7, albedo->get_id(), *albedo->get_view(), vuk::ImageLayout::eReadOnlyOptimalKHR);
      if (normal && normal->is_valid_id())
        descriptor_set_00->update_sampled_image(7, normal->get_id(), *normal->get_view(), vuk::ImageLayout::eReadOnlyOptimalKHR);
      if (physical && physical->is_valid_id())
        descriptor_set_00->update_sampled_image(7, physical->get_id(), *physical->get_view(), vuk::ImageLayout::eReadOnlyOptimalKHR);
      if (ao && ao->is_valid_id())
        descriptor_set_00->update_sampled_image(7, ao->get_id(), *ao->get_view(), vuk::ImageLayout::eReadOnlyOptimalKHR);
      if (emissive && emissive->is_valid_id())
        descriptor_set_00->update_sampled_image(7, emissive->get_id(), *emissive->get_view(), vuk::ImageLayout::eReadOnlyOptimalKHR);
    }
  }

  if (material_parameters.empty())
    material_parameters.emplace_back();

  auto [matBuff, matBufferFut] = create_cpu_buffer(allocator, std::span(material_parameters));
  auto& mat_buffer = *matBuff;

  if (scene_lights.empty())
    scene_lights.emplace_back();

  light_datas.reserve(scene_lights.size());

  const Vec2 atlas_dim_rcp = Vec2(1.0f / float(shadow_map_atlas.get_extent().width), 1.0f / float(shadow_map_atlas.get_extent().height));

  for (auto& lc : scene_lights) {
    auto& light = light_datas.emplace_back(LightData{
      .position = lc.position,
      .intensity = lc.intensity,
      .color = lc.color,
      .radius = lc.range,
      .rotation = lc.rotation,
      .type = (uint32_t)lc.type,
      .direction = lc.direction,
      .cascade_count = (uint32_t)lc.cascade_distances.size(),
    });

    light.shadow_atlas_mul_add.x = lc.shadow_rect.w * atlas_dim_rcp.x;
    light.shadow_atlas_mul_add.y = lc.shadow_rect.h * atlas_dim_rcp.y;
    light.shadow_atlas_mul_add.z = lc.shadow_rect.x * atlas_dim_rcp.x;
    light.shadow_atlas_mul_add.w = lc.shadow_rect.y * atlas_dim_rcp.y;
  }

  std::vector<ShaderEntity> shader_entities = {};

  for (uint32_t light_index = 0; light_index < light_datas.size(); ++light_index) {
    auto& light = light_datas[light_index];
    const auto& lc = scene_lights[light_index];

    if (lc.cast_shadows) {
      auto sh_cameras = std::vector<CameraSH>(light.cascade_count);
      create_dir_light_cameras(lc, *current_camera, sh_cameras, light.cascade_count);

      light.matrix_index = (uint32_t)shader_entities.size();
      for (uint32_t cascade = 0; cascade < light.cascade_count; ++cascade) {
        shader_entities.emplace_back(sh_cameras[cascade].projection_view);
      }
    }
  }

  if (shader_entities.empty())
    shader_entities.emplace_back();

  auto [seBuff, seFut] = create_cpu_buffer(allocator, std::span(shader_entities));
  const auto& shader_entities_buffer = *seBuff;

  auto [lights_buff, lights_buff_fut] = create_cpu_buffer(allocator, std::span(light_datas));
  const auto& lights_buffer = *lights_buff;

  std::vector<MeshInstance> mesh_instances = {};
  mesh_instances.reserve(mesh_component_list.size());
  for (const auto& mc : mesh_component_list) {
    mesh_instances.emplace_back(mc.transform);
  }

  if (mesh_instances.empty())
    mesh_instances.emplace_back();

  auto [instBuff, instanceBuffFut] = create_cpu_buffer(allocator, std::span(mesh_instances));
  const auto& mesh_instances_buffer = *instBuff;

  descriptor_set_00->update_uniform_buffer(0, 0, scene_buffer);
  descriptor_set_00->update_storage_buffer(1, LIGHTS_BUFFER_INDEX, lights_buffer);
  descriptor_set_00->update_storage_buffer(1, MATERIALS_BUFFER_INDEX, mat_buffer);
  descriptor_set_00->update_storage_buffer(1, MESH_INSTANCES_BUFFER_INDEX, mesh_instances_buffer);
  descriptor_set_00->update_storage_buffer(1, ENTITIES_BUFFER_INDEX, shader_entities_buffer);

  // scene textures
  descriptor_set_00->update_sampled_image(2, PBR_IMAGE_INDEX, *pbr_texture.get_view(), vuk::ImageLayout::eReadOnlyOptimalKHR);
  descriptor_set_00->update_sampled_image(2, NORMAL_IMAGE_INDEX, *normal_texture.get_view(), vuk::ImageLayout::eReadOnlyOptimalKHR);
  descriptor_set_00->update_sampled_image(2, DEPTH_IMAGE_INDEX, *depth_texture.get_view(), vuk::ImageLayout::eReadOnlyOptimalKHR);
  descriptor_set_00->update_sampled_image(2, SHADOW_ATLAS_INDEX, *shadow_map_atlas.get_view(), vuk::ImageLayout::eReadOnlyOptimalKHR);
  descriptor_set_00->update_sampled_image(2, SKY_TRANSMITTANCE_LUT_INDEX, *sky_transmittance_lut.get_view(), vuk::ImageLayout::eReadOnlyOptimalKHR);
  descriptor_set_00->update_sampled_image(2, SKY_MULTISCATTER_LUT_INDEX, *sky_multiscatter_lut.get_view(), vuk::ImageLayout::eReadOnlyOptimalKHR);

  // scene uint texture array
  descriptor_set_00->update_sampled_image(3, GTAO_BUFFER_IMAGE_INDEX, *gtao_final_texture.get_view(), vuk::ImageLayout::eReadOnlyOptimalKHR);

  // scene cubemap texture array
  descriptor_set_00->update_sampled_image(4, SKY_ENVMAP_INDEX, *sky_envmap_texture_mipped.get_view(), vuk::ImageLayout::eReadOnlyOptimalKHR);

  // scene Read/Write textures
  descriptor_set_00->update_storage_image(6, SKY_TRANSMITTANCE_LUT_INDEX, *sky_transmittance_lut.get_view());
  descriptor_set_00->update_storage_image(6, SKY_MULTISCATTER_LUT_INDEX, *sky_multiscatter_lut.get_view());
  descriptor_set_00->update_storage_image(6, VELOCITY_IMAGE_INDEX, *velocity_texture.get_view());

  descriptor_set_00->commit(ctx);
}

void DefaultRenderPipeline::create_static_resources(vuk::Allocator& allocator) {
  OX_SCOPED_ZONE;

  constexpr auto transmittance_lut_size = vuk::Extent3D{256, 64, 1};
  sky_transmittance_lut.create_texture(transmittance_lut_size, vuk::Format::eR32G32B32A32Sfloat);

  constexpr auto multi_scatter_lut_size = vuk::Extent3D{32, 32, 1};
  sky_multiscatter_lut.create_texture(multi_scatter_lut_size, vuk::Format::eR32G32B32A32Sfloat);

  constexpr auto shadow_size = vuk::Extent3D{1u, 1u, 1};
  shadow_map_atlas.create_texture(shadow_size, vuk::Format::eD32Sfloat);
  shadow_map_atlas_transparent.create_texture(shadow_size, vuk::Format::eD32Sfloat);

  constexpr auto envmap_size = vuk::Extent3D{512, 512, 1};
  sky_envmap_render_target.create_texture(envmap_size, vuk::Format::eR32G32B32A32Sfloat, vuk::ImageAttachment::Preset::eMapCube);
  sky_envmap_texture_mipped.create_texture(envmap_size, vuk::Format::eR32G32B32A32Sfloat, vuk::ImageAttachment::Preset::eMapCube);
}

void DefaultRenderPipeline::create_dynamic_textures(vuk::Allocator& allocator, const vuk::Extent3D& ext) {
  if (gtao_final_texture.get_extent() != ext)
    gtao_final_texture.create_texture(extent, vuk::Format::eR8Uint);
  if (ssr_texture.get_extent() != ext)
    ssr_texture.create_texture(ext, vuk::Format::eR32G32B32A32Sfloat);
  if (pbr_texture.get_extent() != ext)
    pbr_texture.create_texture(ext, vuk::Format::eR32G32B32A32Sfloat);
  if (normal_texture.get_extent() != ext)
    normal_texture.create_texture(ext, vuk::Format::eR32G32B32A32Sfloat);
  if (depth_texture.get_extent() != ext)
    depth_texture.create_texture(ext, vuk::Format::eD32Sfloat);
  if (velocity_texture.get_extent() != ext)
    velocity_texture.create_texture(ext, vuk::Format::eR16G16Sfloat);

  // Shadow atlas packing:
  {
    OX_SCOPED_ZONE_N("Shadow atlas packing");
    thread_local RectPacker::State packer;
    float iterative_scaling = 1;

    while (iterative_scaling > 0.03f) {
      packer.clear();
      for (uint32_t lightIndex = 0; lightIndex < scene_lights.size(); lightIndex++) {
        LightComponent& light = scene_lights[lightIndex];
        light.shadow_rect = {};
        if (!light.cast_shadows)
          continue;

        const float dist = distance(current_camera->get_position(), light.position);
        const float range = light.range;
        const float amount = std::min(1.0f, range / std::max(0.001f, dist)) * iterative_scaling;

        constexpr int max_shadow_resolution_2D = 1024;
        constexpr int max_shadow_resolution_cube = 256;

        RectPacker::Rect rect = {};
        rect.id = int(lightIndex);
        switch (light.type) {
          case LightComponent::Directional:
            if (light.shadow_map_res > 0) {
              rect.w = light.shadow_map_res * int(light.cascade_distances.size());
              rect.h = light.shadow_map_res;
            } else {
              rect.w = int(max_shadow_resolution_2D * iterative_scaling) * int(light.cascade_distances.size());
              rect.h = int(max_shadow_resolution_2D * iterative_scaling);
            }
            break;
          case LightComponent::Spot:
            if (light.shadow_map_res > 0) {
              rect.w = int(light.shadow_map_res);
              rect.h = int(light.shadow_map_res);
            } else {
              rect.w = int(max_shadow_resolution_2D * amount);
              rect.h = int(max_shadow_resolution_2D * amount);
            }
            break;
          case LightComponent::Point:
            if (light.shadow_map_res > 0) {
              rect.w = int(light.shadow_map_res) * 6;
              rect.h = int(light.shadow_map_res);
            } else {
              rect.w = int(max_shadow_resolution_cube * amount) * 6;
              rect.h = int(max_shadow_resolution_cube * amount);
            }
            break;
        }
        if (rect.w > 8 && rect.h > 8) {
          packer.add_rect(rect);
        }
      }
      if (!packer.rects.empty()) {
        if (packer.pack(8192)) {
          for (auto& rect : packer.rects) {
            if (rect.id == -1) {
              continue;
            }
            uint32_t light_index = uint32_t(rect.id);
            LightComponent& light = scene_lights[light_index];
            if (rect.was_packed) {
              light.shadow_rect = rect;

              // Remove slice multipliers from rect:
              switch (light.type) {
                case LightComponent::Directional: light.shadow_rect.w /= int(light.cascade_distances.size()); break;
                case LightComponent::Point      : light.shadow_rect.w /= 6; break;
              }
            } else {
              light.direction = {};
            }
          }

          if ((int)shadow_map_atlas.get_extent().width < packer.width || (int)shadow_map_atlas.get_extent().height < packer.height) {
            const auto shadow_size = vuk::Extent3D{(uint32_t)packer.width, (uint32_t)packer.height, 1};

            shadow_map_atlas.create_texture(shadow_size, vuk::Format::eD32Sfloat);
            shadow_map_atlas_transparent.create_texture(shadow_size, vuk::Format::eD32Sfloat);

            scene_data.shadow_atlas_res = UVec2(shadow_map_atlas.get_extent().width, shadow_map_atlas.get_extent().height);
          }

          break;
        }

        iterative_scaling *= 0.5f;
      } else {
        iterative_scaling = 0.0; // PE: fix - endless loop if some lights do not have shadows.
      }
    }
  }
}

void DefaultRenderPipeline::create_descriptor_sets(vuk::Allocator& allocator) {
  auto& ctx = allocator.get_context();
  descriptor_set_00 = ctx.create_persistent_descriptorset(allocator, *ctx.get_named_pipeline("pbr_pipeline"), 0, 64);

  const vuk::Sampler linear_sampler_clamped = ctx.acquire_sampler(vuk::LinearSamplerClamped, ctx.get_frame_count());
  const vuk::Sampler linear_sampler_repeated = ctx.acquire_sampler(vuk::LinearSamplerRepeated, ctx.get_frame_count());
  const vuk::Sampler linear_sampler_repeated_anisotropy = ctx.acquire_sampler(vuk::LinearSamplerRepeatedAnisotropy, ctx.get_frame_count());
  const vuk::Sampler nearest_sampler_clamped = ctx.acquire_sampler(vuk::NearestSamplerClamped, ctx.get_frame_count());
  const vuk::Sampler nearest_sampler_repeated = ctx.acquire_sampler(vuk::NearestSamplerRepeated, ctx.get_frame_count());
  const vuk::Sampler cmp_depth_sampler = ctx.acquire_sampler(vuk::CmpDepthSampler, ctx.get_frame_count());
  descriptor_set_00->update_sampler(8, 0, linear_sampler_clamped);
  descriptor_set_00->update_sampler(8, 1, linear_sampler_repeated);
  descriptor_set_00->update_sampler(8, 2, linear_sampler_repeated_anisotropy);
  descriptor_set_00->update_sampler(8, 3, nearest_sampler_clamped);
  descriptor_set_00->update_sampler(8, 4, nearest_sampler_repeated);
  descriptor_set_00->update_sampler(9, 0, cmp_depth_sampler);
}

void DefaultRenderPipeline::run_static_passes(vuk::Allocator& allocator) {
  vuk::Compiler compiler{};

  auto transmittance_fut = sky_transmittance_pass();
  transmittance_fut.wait(allocator, compiler);

  auto multiscatter_fut = sky_multiscatter_pass();
  multiscatter_fut.wait(allocator, compiler);

  ran_static_passes = true;
}

void DefaultRenderPipeline::on_dispatcher_events(EventDispatcher& dispatcher) {
  dispatcher.sink<SkyboxLoadEvent>().connect<&DefaultRenderPipeline::update_skybox>(*this);
}

void DefaultRenderPipeline::register_mesh_component(const MeshComponent& render_object) {
  OX_SCOPED_ZONE;

  if (!current_camera)
    return;

  render_queue.add(render_object.mesh_id,
                   (uint32_t)mesh_component_list.size(),
                   0,
                   distance(current_camera->get_position(), render_object.aabb.get_center()),
                   0);
  mesh_component_list.emplace_back(render_object);
}

void DefaultRenderPipeline::register_light(const LightComponent& light) {
  OX_SCOPED_ZONE;
  auto& lc = scene_lights.emplace_back(light);
  if (light.type == LightComponent::LightType::Directional)
    dir_light_data = &lc;
}

void DefaultRenderPipeline::register_camera(Camera* camera) {
  OX_SCOPED_ZONE;
  current_camera = camera;
}

void DefaultRenderPipeline::shutdown() {}

Unique<vuk::Future> DefaultRenderPipeline::on_render(vuk::Allocator& frame_allocator, const vuk::Future& target, vuk::Dimension3D dim) {
  OX_SCOPED_ZONE;
  if (!current_camera) {
    OX_LOG_ERROR("No camera is set for rendering!");
    // set a temporary one
    if (!default_camera)
      default_camera = create_shared<Camera>();
    current_camera = default_camera.get();
  }

  auto vk_context = VkContext::get();

  ankerl::unordered_dense::map<uint32_t, uint32_t> material_map; // mesh, material

  for (auto& batch : render_queue.batches) {
    material_map.emplace(batch.component_index, (uint32_t)mesh_component_list[batch.component_index].materials.size());
  }

  const auto rg = create_shared<vuk::RenderGraph>("DefaultRPRenderGraph");

  // dummy images
  rg->attach_and_clear_image("black_image",
                             {.format = vk_context->swapchain->format, .sample_count = vuk::SampleCountFlagBits::e1},
                             vuk::Black<float>);
  rg->inference_rule("black_image", vuk::same_shape_as("final_image"));

  rg->attach_and_clear_image("black_image_uint", {.format = vuk::Format::eR8Uint, .sample_count = vuk::SampleCountFlagBits::e1}, vuk::Black<float>);
  rg->inference_rule("black_image_uint", vuk::same_shape_as("final_image"));

  rg->attach_image("sky_transmittance_lut+", vuk::ImageAttachment::from_texture(sky_transmittance_lut));
  rg->attach_image("sky_multiscatter_lut+", vuk::ImageAttachment::from_texture(sky_multiscatter_lut));

  Vec3 sun_direction = {0, 1, 0};
  Vec3 sun_color = {};

  if (dir_light_data) {
    sun_direction = dir_light_data->direction;
    sun_color = dir_light_data->color * dir_light_data->intensity;
  }

  scene_data.sun_direction = sun_direction;
  scene_data.sun_color = Vec4(sun_color, 1.0f);

  create_dynamic_textures(*vk_context->superframe_allocator, dim);
  update_frame_data(frame_allocator);

  if (!ran_static_passes) {
    run_static_passes(*vk_context->superframe_allocator);
  }

  if (dir_light_data && dir_light_data->cast_shadows) {
    auto attachment = vuk::ImageAttachment::from_texture(shadow_map_atlas);
    rg->attach_and_clear_image("shadow_map", attachment, vuk::DepthZero);
    shadow_pass(rg);
  } else {
    rg->attach_and_clear_image("shadow_map_output", vuk::ImageAttachment::from_texture(shadow_map_atlas), vuk::DepthZero);
  }

  depth_pre_pass(rg);

  if (RendererCVar::cvar_gtao_enable.get())
    gtao_pass(frame_allocator, rg);

#if 0 // unused
  sky_view_lut_pass(rg);
#endif

  sky_envmap_pass(rg);

  geometry_pass(rg);

  if (RendererCVar::cvar_bloom_enable.get())
    bloom_pass(rg);

  // TODO: use bindless(SceneData etc) for the passes below

  auto [final_buff, final_buff_fut] = create_cpu_buffer(frame_allocator, std::span(&scene_data.post_processing_data, 1));
  auto& final_buffer = *final_buff;

  auto [bloom_resource, bloom_name] = get_attachment_or_black("bloom_upsampled_image", RendererCVar::cvar_bloom_enable.get());

  vuk::Name pbr_image_name = "pbr_output";

  if (RendererCVar::cvar_fxaa_enable.get()) {
    struct FXAAData {
      Vec2 inverse_screen_size;
    } fxaa_data;

    fxaa_data.inverse_screen_size = 1.0f / Vec2((float)Renderer::get_viewport_width(), (float)Renderer::get_viewport_height());
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

  rg->add_pass({.name = "final_pass",
                .resources = std::move(final_resources),
                .execute = [this, final_buffer, bloom_name, pbr_image_name](vuk::CommandBuffer& command_buffer) {
    clear();
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
  }});

  return create_unique<vuk::Future>(rg, "final_output");
}

void DefaultRenderPipeline::on_update(Scene* scene) {
  // TODO: Account for the bounding volume of the probe
  const auto pp_view = scene->registry.view<PostProcessProbe>();
  for (auto&& [e, component] : pp_view.each()) {
    scene_data.post_processing_data.film_grain = {component.film_grain_enabled, component.film_grain_intensity};
    scene_data.post_processing_data.chromatic_aberration = {component.chromatic_aberration_enabled, component.chromatic_aberration_intensity};
    scene_data.post_processing_data.vignette_offset.w = component.vignette_enabled;
    scene_data.post_processing_data.vignette_color.a = component.vignette_intensity;
    scene_data.post_processing_data.sharpen.x = component.sharpen_enabled;
    scene_data.post_processing_data.sharpen.y = component.sharpen_intensity;
  }
}

void DefaultRenderPipeline::update_skybox(const SkyboxLoadEvent& e) {
  OX_SCOPED_ZONE;
  cube_map = e.cube_map;

  if (cube_map)
    generate_prefilter(*VkContext::get()->superframe_allocator);
}

void DefaultRenderPipeline::render_meshes(const RenderQueue& render_queue,
                                          vuk::CommandBuffer& command_buffer,
                                          uint32_t filter,
                                          uint32_t flags,
                                          uint32_t camera_count) const {
  const size_t alloc_size = render_queue.size() * camera_count * sizeof(MeshInstancePointer);
  const auto& instances = command_buffer._map_scratch_buffer(1, 1, alloc_size);

  struct InstancedBatch {
    uint32_t mesh_index = 0;
    uint32_t component_index = 0;
    uint32_t instance_count = 0;
    uint32_t data_offset = 0;
    uint32_t lod = 0;
  };

  InstancedBatch instanced_batch = {};

  const auto flush_batch = [&] {
    if (instanced_batch.instance_count == 0)
      return;

    const auto& mesh = mesh_component_list[instanced_batch.component_index];

    if (flags & RENDER_FLAGS_SHADOWS_PASS && !mesh.cast_shadows) {
      return;
    }

    const auto node = mesh.get_linear_node();

    mesh.mesh_base->bind_index_buffer(command_buffer);

    uint32_t primitive_index = 0;
    for (const auto primitive : node->mesh_data->primitives) {
      auto& material = mesh.materials[primitive_index];
      if (filter & FILTER_TRANSPARENT) {
        if (material->parameters.alpha_mode == (uint32_t)Material::AlphaMode::Blend) {
          continue;
        }
      }
      if (filter & FILTER_CLIP) {
        if (material->parameters.alpha_mode == (uint32_t)Material::AlphaMode::Mask) {
          continue;
        }
      }
      if (filter & FILTER_OPAQUE) {
        if (material->parameters.alpha_mode != (uint32_t)Material::AlphaMode::Blend) {
          continue;
        }
      }

      const auto pc = ShaderPC{
        mesh.mesh_base->vertex_buffer->device_address,
        instanced_batch.data_offset,
        instanced_batch.component_index + primitive_index,
      };

      vuk::ShaderStageFlags stage = vuk::ShaderStageFlagBits::eVertex;
      if (!(flags & RENDER_FLAGS_SHADOWS_PASS))
        stage = stage | vuk::ShaderStageFlagBits::eFragment;
      command_buffer.push_constants(stage, 0, pc);
      command_buffer.draw_indexed(primitive->index_count, instanced_batch.instance_count, primitive->first_index, 0, 0);

      primitive_index++;
    }

    const auto pc = ShaderPC{
      .vertex_buffer_ptr = mesh.mesh_base->vertex_buffer->device_address,
      .mesh_index = instanced_batch.data_offset,
      .material_index = instanced_batch.component_index, // + draw_index in hlsl
    };

    vuk::ShaderStageFlags stage = vuk::ShaderStageFlagBits::eVertex;
    if (!(flags & RENDER_FLAGS_SHADOWS_PASS))
      stage = stage | vuk::ShaderStageFlagBits::eFragment;
    command_buffer.push_constants(stage, 0, pc);
  };

  uint32_t instance_count = 0;
  for (const auto& batch : render_queue.batches) {
    const auto instance_index = batch.get_instance_index();

    const auto& mats1 = mesh_component_list[batch.component_index].materials;
    const auto& mats2 = mesh_component_list[instanced_batch.component_index].materials;
    bool materials_match;
    // don't have to check if they are different sized
    if (mats1.size() != mats2.size()) {
      materials_match = false;
    } else {
      materials_match = std::equal(mats1.begin(), mats1.end(), mats2.begin(), [](const Shared<Material>& mat1, const Shared<Material>& mat2) {
        return *mat1.get() == *mat2.get();
      });
    }

    if (batch.mesh_index != instanced_batch.mesh_index || !materials_match) {
      flush_batch();

      instanced_batch = {};
      instanced_batch.mesh_index = batch.mesh_index;
      instanced_batch.data_offset = instance_count;
    }

    for (uint32_t camera_index = 0; camera_index < camera_count; ++camera_index) {
      const uint16_t camera_mask = 1 << camera_index;
      if ((batch.camera_mask & camera_mask) == 0)
        continue;

      MeshInstancePointer poi;
      poi.Create(instance_index, camera_index, 0 /*dither*/);
      std::memcpy((MeshInstancePointer*)instances + instance_count, &poi, sizeof(poi));

      instanced_batch.component_index = batch.component_index;
      instanced_batch.instance_count++;
      instance_count++;
    }
  }

  flush_batch();
}

void DefaultRenderPipeline::shadow_pass(const Shared<vuk::RenderGraph>& rg) {
  OX_SCOPED_ZONE;

  const auto resources = std::vector{
    "shadow_map"_image >> vuk::eDepthStencilRW >> "shadow_map_output",
  };

  rg->add_pass({.name = "shadow_pass", .resources = resources, .execute = [this](vuk::CommandBuffer& command_buffer) {
    command_buffer.bind_persistent(0, *descriptor_set_00)
      .bind_graphics_pipeline("shadow_pipeline")
      .set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
      .broadcast_color_blend({})
      .set_rasterization({.cullMode = vuk::CullModeFlagBits::eBack})
      .set_depth_stencil(vuk::PipelineDepthStencilStateCreateInfo{
        .depthTestEnable = true,
        .depthWriteEnable = true,
        .depthCompareOp = vuk::CompareOp::eGreaterOrEqual,
      });

    const auto max_viewport_count = VkContext::get()->get_max_viewport_count();
    for (auto& light : scene_lights) {
      switch (light.type) {
        case LightComponent::Directional: {
          const uint32_t cascade_count = std::min((uint32_t)light.cascade_distances.size(), max_viewport_count);
          auto viewports = std::vector<vuk::Viewport>(cascade_count);
          auto cameras = std::vector<CameraData>(cascade_count);
          auto sh_cameras = std::vector<CameraSH>(cascade_count);
          create_dir_light_cameras(light, *current_camera, sh_cameras, cascade_count);

          RenderQueue shadow_queue = {};
          uint32_t batch_index = 0;
          for (auto& batch : render_queue.batches) {
            // Determine which cascades the object is contained in:
            uint16_t camera_mask = 0;
            for (uint32_t cascade = 0; cascade < cascade_count; ++cascade) {
              const auto frustum = sh_cameras[cascade].frustum;
              const auto aabb = mesh_component_list[batch.component_index].aabb;
              if (cascade < cascade_count && true /* aabb.is_on_frustum(frustum) */) {
                camera_mask |= 1 << cascade;
              }
            }

            if (camera_mask == 0) {
              continue;
            }

            auto& b = shadow_queue.add(batch);
            b.instance_index = batch_index;
            b.camera_mask = camera_mask;

            batch_index++;
          }

          if (!shadow_queue.empty()) {
            for (uint32_t cascade = 0; cascade < cascade_count; ++cascade) {
              cameras[cascade].projection_view = sh_cameras[cascade].projection_view;
              cameras[cascade].output_index = cascade;
              camera_cb.camera_data[cascade] = cameras[cascade];

              auto& vp = viewports[cascade];
              vp.x = float(light.shadow_rect.x + cascade * light.shadow_rect.w);
              vp.y = float(light.shadow_rect.y);
              vp.width = float(light.shadow_rect.w);
              vp.height = float(light.shadow_rect.h);
              vp.minDepth = 0.0f;
              vp.maxDepth = 1.0f;

              command_buffer.set_scissor(cascade, vuk::Rect2D::framebuffer());
              command_buffer.set_viewport(cascade, vp);
            }

            bind_camera_buffer(command_buffer);

            shadow_queue.sort_opaque();

            render_meshes(shadow_queue, command_buffer, FILTER_TRANSPARENT, RENDER_FLAGS_SHADOWS_PASS, cascade_count);
          }

          break;
        }
        case LightComponent::Point: {
          break;
        }
        case LightComponent::Spot: {
          break;
        }
      }
    }
  }});
}

void DefaultRenderPipeline::depth_pre_pass(const Shared<vuk::RenderGraph>& rg) {
  OX_SCOPED_ZONE;

  rg->attach_and_clear_image("normal_image", vuk::ImageAttachment::from_texture(normal_texture), vuk::Black<float>);
  rg->attach_and_clear_image("depth_image", vuk::ImageAttachment::from_texture(depth_texture), vuk::DepthZero);
  rg->attach_and_clear_image("velocity_image", vuk::ImageAttachment::from_texture(velocity_texture), vuk::Black<float>);

  const auto resources = std::vector{
    "normal_image"_image >> vuk::eColorRW >> "normal_output",
    "velocity_image"_image >> vuk::eColorRW >> "velocity_output",
    "depth_image"_image >> vuk::eDepthStencilRW >> "depth_output",
  };

  rg->add_pass({.name = "depth_pre_pass", .resources = resources, .execute = [this](vuk::CommandBuffer& command_buffer) {
    command_buffer.bind_persistent(0, *descriptor_set_00)
      .set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
      .set_viewport(0, vuk::Rect2D::framebuffer())
      .set_scissor(0, vuk::Rect2D::framebuffer())
      .broadcast_color_blend(vuk::BlendPreset::eOff)
      .set_rasterization({.cullMode = vuk::CullModeFlagBits::eBack})
      .set_depth_stencil(vuk::PipelineDepthStencilStateCreateInfo{
        .depthTestEnable = true,
        .depthWriteEnable = true,
        .depthCompareOp = vuk::CompareOp::eGreaterOrEqual,
      })
      .bind_graphics_pipeline("depth_pre_pass_pipeline");

    camera_cb.camera_data[0] = get_main_camera_data();
    bind_camera_buffer(command_buffer);

    RenderQueue prepass_queue = {};
    const auto camera_frustum = current_camera->get_frustum();
    for (uint32_t batch_index = 0; batch_index < render_queue.batches.size(); batch_index++) {
      const auto& batch = render_queue.batches[batch_index];
      const auto& mc = mesh_component_list.at(batch.component_index);
      if (!mc.aabb.is_on_frustum(camera_frustum)) {
        continue;
      }

      auto& b = prepass_queue.add(batch);
      b.instance_index = batch_index;
    }

    prepass_queue.sort_opaque();

    render_meshes(prepass_queue, command_buffer, FILTER_TRANSPARENT);
  }});
}

void DefaultRenderPipeline::geometry_pass(const Shared<vuk::RenderGraph>& rg) {
  OX_SCOPED_ZONE;

  rg->attach_and_clear_image("pbr_image", vuk::ImageAttachment::from_texture(pbr_texture), vuk::Black<float>);

  auto [gtao_resource, gtao_name] = get_attachment_or_black_uint("gtao_final_output", RendererCVar::cvar_gtao_enable.get());

  const auto resources = std::vector<vuk::Resource>{
    "pbr_image"_image >> vuk::eColorRW >> "pbr_output_opaque",
    "depth_output"_image >> vuk::eDepthStencilRead,
    "shadow_map_output"_image >> vuk::eFragmentSampled,
    "sky_transmittance_lut+"_image >> vuk::eFragmentSampled,
    "sky_multiscatter_lut+"_image >> vuk::eFragmentSampled,
    "sky_envmap_image_final"_image >> vuk::eFragmentSampled,
    gtao_resource,
  };

  rg->add_pass({.name = "geometry_pass", .resources = resources, .execute = [this](vuk::CommandBuffer& command_buffer) {
    camera_cb.camera_data[0] = get_main_camera_data();
    bind_camera_buffer(command_buffer);

    command_buffer.bind_persistent(0, *descriptor_set_00)
      .set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
      .set_viewport(0, vuk::Rect2D::framebuffer())
      .set_scissor(0, vuk::Rect2D::framebuffer())
      .broadcast_color_blend(vuk::BlendPreset::eOff)
      .set_depth_stencil({.depthTestEnable = false, .depthWriteEnable = false, .depthCompareOp = vuk::CompareOp::eGreaterOrEqual})
      .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
      .bind_graphics_pipeline("sky_view_final_pipeline")
      .draw(3, 1, 0, 0);

    command_buffer.bind_graphics_pipeline("pbr_pipeline")
      .bind_persistent(0, *descriptor_set_00)
      .set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
      .set_rasterization({.cullMode = vuk::CullModeFlagBits::eBack})
      .set_viewport(0, vuk::Rect2D::framebuffer())
      .set_scissor(0, vuk::Rect2D::framebuffer())
      .broadcast_color_blend({})
      .set_depth_stencil(vuk::PipelineDepthStencilStateCreateInfo{
        .depthTestEnable = true,
        .depthWriteEnable = false,
        .depthCompareOp = vuk::CompareOp::eGreaterOrEqual,
      });

    camera_cb.camera_data[0] = get_main_camera_data();
    bind_camera_buffer(command_buffer);

    RenderQueue geometry_queue = {};
    const auto camera_frustum = current_camera->get_frustum();
    for (uint32_t batch_index = 0; batch_index < render_queue.batches.size(); batch_index++) {
      const auto& batch = render_queue.batches[batch_index];
      const auto& mc = mesh_component_list.at(batch.component_index);
      if (!mc.aabb.is_on_frustum(camera_frustum)) {
        continue;
      }

      auto& b = geometry_queue.add(batch);
      b.instance_index = batch_index;
    }

    geometry_queue.sort_opaque();

    render_meshes(geometry_queue, command_buffer, FILTER_TRANSPARENT);
  }});

  rg->add_pass({.name = "geometry_pass_transparent",
                .resources = {"pbr_output_opaque"_image >> vuk::eColorRW >> "pbr_output"},
                .execute = [this](vuk::CommandBuffer& command_buffer) {
    command_buffer.bind_graphics_pipeline("pbr_transparency_pipeline")
      .bind_persistent(0, *descriptor_set_00)
      .set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
      .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
      .set_viewport(0, vuk::Rect2D::framebuffer())
      .set_scissor(0, vuk::Rect2D::framebuffer())
      .broadcast_color_blend(vuk::BlendPreset::eAlphaBlend)
      .set_depth_stencil(vuk::PipelineDepthStencilStateCreateInfo{
        .depthTestEnable = true,
        .depthWriteEnable = false,
        .depthCompareOp = vuk::CompareOp::eGreaterOrEqual,
      });

    camera_cb.camera_data[0] = get_main_camera_data();
    bind_camera_buffer(command_buffer);

    RenderQueue geometry_queue = {};
    const auto camera_frustum = current_camera->get_frustum();
    for (uint32_t batch_index = 0; batch_index < render_queue.batches.size(); batch_index++) {
      const auto& batch = render_queue.batches[batch_index];
      const auto& mc = mesh_component_list.at(batch.component_index);
      if (!mc.aabb.is_on_frustum(camera_frustum)) {
        continue;
      }

      auto& b = geometry_queue.add(batch);
      b.instance_index = batch_index;
    }

    geometry_queue.sort_transparent();

    render_meshes(geometry_queue, command_buffer, FILTER_OPAQUE);
  }});
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

  rg->attach_and_clear_image("bloom_image",
                             {.format = vuk::Format::eR32G32B32A32Sfloat,
                              .sample_count = vuk::SampleCountFlagBits::e1,
                              .level_count = bloom_mip_count,
                              .layer_count = 1},
                             vuk::Black<float>);
  rg->inference_rule("bloom_image", vuk::same_extent_as("final_image"));

  auto [down_input_names, down_output_names] = diverge_image_mips(rg, "bloom_image", bloom_mip_count);

  rg->add_pass({.name = "bloom_prefilter",
                .resources = {vuk::Resource(down_input_names[0], vuk::Resource::Type::eImage, vuk::eComputeRW, down_output_names[0]),
                              "pbr_output"_image >> vuk::eComputeSampled},
                .execute = [bloom_push_const, down_input_names](vuk::CommandBuffer& command_buffer) {
    command_buffer.bind_compute_pipeline("bloom_prefilter_pipeline")
      .push_constants(vuk::ShaderStageFlagBits::eCompute, 0, bloom_push_const)
      .bind_image(0, 0, down_input_names[0])
      .bind_sampler(0, 0, vuk::NearestMagLinearMinSamplerClamped)
      .bind_image(0, 1, "pbr_output")
      .bind_sampler(0, 1, vuk::NearestMagLinearMinSamplerClamped)
      .dispatch((Renderer::get_viewport_width() + 8 - 1) / 8, (Renderer::get_viewport_height() + 8 - 1) / 8, 1);
  }});

  for (int32_t i = 1; i < (int32_t)bloom_mip_count; i++) {
    auto& input_name = down_output_names[i - 1];
    rg->add_pass({.name = "bloom_downsample",
                  .resources =
                    {
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
    }});
  }

  // Upsampling
  // https://www.froyok.fr/blog/2021-12-ue4-custom-bloom/resources/code/bloom_down_up_demo.jpg

  rg->attach_and_clear_image("bloom_upsample_image",
                             {.format = vuk::Format::eR32G32B32A32Sfloat,
                              .sample_count = vuk::SampleCountFlagBits::e1,
                              .level_count = bloom_mip_count - 1,
                              .layer_count = 1},
                             vuk::Black<float>);
  rg->inference_rule("bloom_upsample_image", vuk::same_extent_as("final_image"));

  auto [up_diverged_names, up_output_names] = diverge_image_mips(rg, "bloom_upsample_image", bloom_mip_count - 1);

  for (int32_t i = (int32_t)bloom_mip_count - 2; i >= 0; i--) {
    const auto input_name = i == (int32_t)bloom_mip_count - 2 ? down_output_names[i + 1] : up_output_names[i + 1];
    rg->add_pass({.name = "bloom_upsample",
                  .resources = {vuk::Resource(up_diverged_names[i], vuk::Resource::Type::eImage, vuk::eComputeRW, up_output_names[i]),
                                vuk::Resource(input_name, vuk::Resource::Type::eImage, vuk::eComputeSampled),
                                vuk::Resource(down_output_names[i], vuk::Resource::Type::eImage, vuk::eComputeSampled),
                                vuk::Resource(down_output_names.back(), vuk::Resource::Type::eImage, vuk::eComputeSampled)},
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
    }});
  }

  rg->converge_image_explicit(up_output_names, "bloom_upsampled_image");
}

void DefaultRenderPipeline::gtao_pass(vuk::Allocator& frame_allocator, const Shared<vuk::RenderGraph>& rg) {
  OX_SCOPED_ZONE;
  gtao_settings.quality_level = RendererCVar::cvar_gtao_quality_level.get();
  gtao_settings.denoise_passes = RendererCVar::cvar_gtao_denoise_passes.get();
  gtao_settings.radius = RendererCVar::cvar_gtao_radius.get();
  gtao_settings.radius_multiplier = 1.0f;
  gtao_settings.falloff_range = RendererCVar::cvar_gtao_falloff_range.get();
  gtao_settings.sample_distribution_power = RendererCVar::cvar_gtao_sample_distribution_power.get();
  gtao_settings.thin_occluder_compensation = RendererCVar::cvar_gtao_thin_occluder_compensation.get();
  gtao_settings.final_value_power = RendererCVar::cvar_gtao_final_value_power.get();
  gtao_settings.depth_mip_sampling_offset = RendererCVar::cvar_gtao_depth_mip_sampling_offset.get();

  gtao_update_constants(gtao_constants, (int)Renderer::get_viewport_width(), (int)Renderer::get_viewport_height(), gtao_settings, current_camera, 0);

  auto [gtao_const_buff, gtao_const_buff_fut] = create_cpu_buffer(frame_allocator, std::span(&gtao_constants, 1));
  auto& gtao_const_buffer = *gtao_const_buff;

  rg->attach_and_clear_image("gtao_depth_image",
                             {.format = vuk::Format::eR32Sfloat, .sample_count = vuk::SampleCountFlagBits::e1, .level_count = 5, .layer_count = 1},
                             vuk::Black<float>);
  rg->inference_rule("gtao_depth_image", vuk::same_extent_as("final_image"));
  auto [diverged_names, output_names] = diverge_image_mips(rg, "gtao_depth_image", 5);

  rg->add_pass({.name = "gtao_first_pass",
                .resources = {vuk::Resource(diverged_names[0], vuk::Resource::Type::eImage, vuk::eComputeRW, output_names[0]),
                              vuk::Resource(diverged_names[1], vuk::Resource::Type::eImage, vuk::eComputeRW, output_names[1]),
                              vuk::Resource(diverged_names[2], vuk::Resource::Type::eImage, vuk::eComputeRW, output_names[2]),
                              vuk::Resource(diverged_names[3], vuk::Resource::Type::eImage, vuk::eComputeRW, output_names[3]),
                              vuk::Resource(diverged_names[4], vuk::Resource::Type::eImage, vuk::eComputeRW, output_names[4]),
                              "depth_output"_image >> vuk::eComputeSampled},
                .execute = [gtao_const_buffer, diverged_names](vuk::CommandBuffer& command_buffer) {
    command_buffer.bind_compute_pipeline("gtao_first_pipeline")
      .bind_buffer(0, 0, gtao_const_buffer)
      .bind_image(0, 1, "depth_output")
      .bind_image(0, 2, diverged_names[0])
      .bind_image(0, 3, diverged_names[1])
      .bind_image(0, 4, diverged_names[2])
      .bind_image(0, 5, diverged_names[3])
      .bind_image(0, 6, diverged_names[4])
      .bind_sampler(0, 7, vuk::NearestSamplerClamped)
      .dispatch((Renderer::get_viewport_width() + 16 - 1) / 16, (Renderer::get_viewport_height() + 16 - 1) / 16);
  }});

  rg->converge_image_explicit(output_names, "gtao_depth_output");

  rg->add_pass({.name = "gtao_main_pass",
                .resources = {"gtao_main_image"_image >> vuk::eComputeRW >> "gtao_main_output",
                              "gtao_edge_image"_image >> vuk::eComputeRW >> "gtao_edge_output",
                              "gtao_depth_output"_image >> vuk::eComputeSampled,
                              "normal_output"_image >> vuk::eComputeSampled},
                .execute = [gtao_const_buffer](vuk::CommandBuffer& command_buffer) {
    command_buffer.bind_compute_pipeline("gtao_main_pipeline")
      .bind_buffer(0, 0, gtao_const_buffer)
      .bind_image(0, 1, "gtao_depth_output")
      .bind_image(0, 2, "normal_output")
      .bind_image(0, 3, "gtao_main_image")
      .bind_image(0, 4, "gtao_edge_image")
      .bind_sampler(0, 5, vuk::NearestSamplerClamped)
      .dispatch((Renderer::get_viewport_width() + 8 - 1) / 8, (Renderer::get_viewport_height() + 8 - 1) / 8);
  }});

  rg->attach_and_clear_image("gtao_main_image",
                             {.format = vuk::Format::eR8Uint,
                              .sample_count = vuk::SampleCountFlagBits::e1,
                              .view_type = vuk::ImageViewType::e2D,
                              .level_count = 1,
                              .layer_count = 1},
                             vuk::Black<float>);
  rg->attach_and_clear_image("gtao_edge_image",
                             {.format = vuk::Format::eR8Unorm,
                              .sample_count = vuk::SampleCountFlagBits::e1,
                              .view_type = vuk::ImageViewType::e2D,
                              .level_count = 1,
                              .layer_count = 1},
                             vuk::Black<float>);
  rg->inference_rule("gtao_main_image", vuk::same_extent_as("final_image"));
  rg->inference_rule("gtao_edge_image", vuk::same_extent_as("final_image"));

  const int pass_count = std::max(1, gtao_settings.denoise_passes); // should be at least one for now.
  auto gtao_denoise_output = vuk::Name(fmt::format("gtao_denoised_image_{}_output", pass_count - 1));
  for (int i = 0; i < pass_count; i++) {
    auto attachment_name = vuk::Name(fmt::format("gtao_denoised_image_{}", i));
    rg->attach_and_clear_image(attachment_name,
                               {.format = vuk::Format::eR8Uint,
                                .sample_count = vuk::SampleCountFlagBits::e1,
                                .view_type = vuk::ImageViewType::e2D,
                                .level_count = 1,
                                .layer_count = 1},
                               vuk::Black<float>);
    rg->inference_rule(attachment_name, vuk::same_extent_as("final_image"));

    auto read_input = i == 0 ? vuk::Name("gtao_main_output") : vuk::Name(fmt::format("gtao_denoised_image_{}_output", i - 1));
    rg->add_pass({.name = "gtao_denoise_pass",
                  .resources = {vuk::Resource(attachment_name, vuk::Resource::Type::eImage, vuk::eComputeRW, attachment_name.append("_output")),
                                vuk::Resource(read_input, vuk::Resource::Type::eImage, vuk::eComputeSampled),
                                "gtao_edge_output"_image >> vuk::eComputeSampled},
                  .execute = [gtao_const_buffer, attachment_name, read_input](vuk::CommandBuffer& command_buffer) {
      command_buffer.bind_compute_pipeline("gtao_denoise_pipeline")
        .bind_buffer(0, 0, gtao_const_buffer)
        .bind_image(0, 1, read_input)
        .bind_image(0, 2, "gtao_edge_output")
        .bind_image(0, 3, attachment_name)
        .bind_sampler(0, 4, vuk::NearestSamplerClamped)
        .dispatch((Renderer::get_viewport_width() + XE_GTAO_NUMTHREADS_X * 2 - 1) / (XE_GTAO_NUMTHREADS_X * 2),
                  (Renderer::get_viewport_height() + XE_GTAO_NUMTHREADS_Y - 1) / XE_GTAO_NUMTHREADS_Y,
                  1);
    }});
  }

  rg->add_pass({.name = "gtao_final_pass",
                .resources = {"gtao_final_image"_image >> vuk::eComputeRW >> "gtao_final_output",
                              vuk::Resource(gtao_denoise_output, vuk::Resource::Type::eImage, vuk::eComputeSampled),
                              "gtao_edge_output"_image >> vuk::eComputeSampled},
                .execute = [gtao_const_buffer, gtao_denoise_output](vuk::CommandBuffer& command_buffer) {
    command_buffer.bind_compute_pipeline("gtao_final_pipeline")
      .bind_buffer(0, 0, gtao_const_buffer)
      .bind_image(0, 1, gtao_denoise_output)
      .bind_image(0, 2, "gtao_edge_output")
      .bind_image(0, 3, "gtao_final_image")
      .bind_sampler(0, 4, vuk::NearestSamplerClamped)
      .dispatch((Renderer::get_viewport_width() + XE_GTAO_NUMTHREADS_X * 2 - 1) / (XE_GTAO_NUMTHREADS_X * 2),
                (Renderer::get_viewport_height() + XE_GTAO_NUMTHREADS_Y - 1) / XE_GTAO_NUMTHREADS_Y,
                1);
  }});

  rg->attach_and_clear_image("gtao_final_image", vuk::ImageAttachment::from_texture(gtao_final_texture), vuk::Black<float>);
  rg->inference_rule("gtao_final_image", vuk::same_extent_as("final_image"));
}

void DefaultRenderPipeline::apply_fxaa(vuk::RenderGraph* rg, const vuk::Name src, const vuk::Name dst, vuk::Buffer& fxaa_buffer) {
  OX_SCOPED_ZONE;
  rg->add_pass({.name = "fxaa",
                .resources =
                  {
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
  }});
}

void DefaultRenderPipeline::apply_grid(vuk::RenderGraph* rg, const vuk::Name dst, const vuk::Name depth_image_name) {
  OX_SCOPED_ZONE;
  rg->add_pass({.name = "grid",
                .resources = {vuk::Resource(dst, vuk::Resource::Type::eImage, vuk::eColorWrite, dst.append("+")),
                              vuk::Resource(depth_image_name, vuk::Resource::Type::eImage, vuk::eDepthStencilRW)},
                .execute = [this](vuk::CommandBuffer& command_buffer) {
    command_buffer.bind_graphics_pipeline("grid_pipeline")
      .set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
      .set_viewport(0, vuk::Rect2D::framebuffer())
      .set_scissor(0, vuk::Rect2D::framebuffer())
      .broadcast_color_blend(vuk::BlendPreset::eAlphaBlend)
      .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
      .set_depth_stencil({.depthTestEnable = true, .depthWriteEnable = false, .depthCompareOp = vuk::CompareOp::eGreaterOrEqual})
      .bind_persistent(0, *descriptor_set_00);

    bind_camera_buffer(command_buffer);

    m_quad->bind_index_buffer(command_buffer)->bind_vertex_buffer(command_buffer);

    command_buffer.draw_indexed(m_quad->index_count, 1, 0, 0, 0);
  }});
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

vuk::Future DefaultRenderPipeline::sky_transmittance_pass() {
  OX_SCOPED_ZONE;

  IVec2 lut_size = {256, 64};

  const auto rg = create_shared<vuk::RenderGraph>("sky_transmittance_pass");

  rg->attach_and_clear_image("sky_transmittance_lut", vuk::ImageAttachment::from_texture(sky_transmittance_lut), vuk::Black<float>);

  const std::vector<vuk::Resource> resources = {"sky_transmittance_lut"_image >> vuk::eComputeRW};

  rg->add_pass({.name = "sky_transmittance_lut_pass", .resources = resources, .execute = [this, lut_size](vuk::CommandBuffer& command_buffer) {
    command_buffer.bind_persistent(0, *descriptor_set_00)
      .bind_compute_pipeline("sky_transmittance_pipeline")
      .dispatch((lut_size.x + 8 - 1) / 8, (lut_size.y + 8 - 1) / 8);
  }});

  return transition(vuk::Future{rg, "sky_transmittance_lut+"}, vuk::eFragmentSampled);
}

vuk::Future DefaultRenderPipeline::sky_multiscatter_pass() {
  OX_SCOPED_ZONE;

  IVec2 lut_size = {32, 32};

  const auto rg = create_shared<vuk::RenderGraph>("sky_multiscatter_pass");

  rg->attach_and_clear_image("sky_multiscatter_lut", vuk::ImageAttachment::from_texture(sky_multiscatter_lut), vuk::Black<float>);

  rg->attach_image("sky_transmittance_lut+", vuk::ImageAttachment::from_texture(sky_transmittance_lut));

  const std::vector<vuk::Resource> resources = {
    "sky_multiscatter_lut"_image >> vuk::eComputeRW,
    "sky_transmittance_lut+"_image >> vuk::eComputeSampled,
  };

  rg->add_pass({.name = "sky_multiscatter_lut_pass", .resources = resources, .execute = [this, lut_size](vuk::CommandBuffer& command_buffer) {
    command_buffer.bind_compute_pipeline("sky_multiscatter_pipeline").bind_persistent(0, *descriptor_set_00).dispatch(lut_size.x, lut_size.y);
  }});

  return transition(vuk::Future{rg, "sky_multiscatter_lut+"}, vuk::eFragmentSampled);
}

void DefaultRenderPipeline::sky_envmap_pass(const Shared<vuk::RenderGraph>& rg) {
  Shared<vuk::RenderGraph> rgp = create_shared<vuk::RenderGraph>("sky_envmap_pass");

  auto attachment = vuk::ImageAttachment::from_texture(sky_envmap_render_target);
  attachment.image_flags = vuk::ImageCreateFlagBits::eCubeCompatible;
  attachment.view_type = vuk::ImageViewType::eCube;
  rgp->attach_and_clear_image("sky_envmap_image", attachment, vuk::Black<float>);

  const std::vector<vuk::Resource> resources = {"sky_envmap_image"_image >> vuk::eColorRW};

  rgp->add_pass({.name = "sky_envmap_pass", .resources = resources, .execute = [this](vuk::CommandBuffer& command_buffer) {
    const auto capture_projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 90.0f);
    const Mat4 capture_views[] = {
      // POSITIVE_X
      rotate(rotate(Mat4(1.0f), glm::radians(90.0f), Vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), Vec3(1.0f, 0.0f, 0.0f)),
      // NEGATIVE_X
      rotate(rotate(Mat4(1.0f), glm::radians(-90.0f), Vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), Vec3(1.0f, 0.0f, 0.0f)),
      // POSITIVE_Y
      rotate(Mat4(1.0f), glm::radians(-90.0f), Vec3(1.0f, 0.0f, 0.0f)),
      // NEGATIVE_Y
      rotate(Mat4(1.0f), glm::radians(90.0f), Vec3(1.0f, 0.0f, 0.0f)),
      // POSITIVE_Z
      rotate(Mat4(1.0f), glm::radians(180.0f), Vec3(1.0f, 0.0f, 0.0f)),
      // NEGATIVE_Z
      rotate(Mat4(1.0f), glm::radians(180.0f), Vec3(0.0f, 0.0f, 1.0f)),
    };

    for (int i = 0; i < 6; i++)
      camera_cb.camera_data[i].projection_view = capture_projection * capture_views[i];

    bind_camera_buffer(command_buffer);

    command_buffer.bind_persistent(0, *descriptor_set_00)
      .set_viewport(0, vuk::Rect2D::framebuffer())
      .set_scissor(0, vuk::Rect2D::framebuffer())
      .broadcast_color_blend(vuk::BlendPreset::eOff)
      .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
      .set_depth_stencil({})
      .bind_graphics_pipeline("sky_envmap_pipeline");

    m_cube->bind_index_buffer(command_buffer)->bind_vertex_buffer(command_buffer);
    command_buffer.draw_indexed(m_cube->index_count, 6, 0, 0, 0);
  }});

  auto attachment_final = vuk::ImageAttachment::from_texture(sky_envmap_texture_mipped);
  attachment_final.image_flags = vuk::ImageCreateFlagBits::eCubeCompatible;
  attachment_final.view_type = vuk::ImageViewType::eCube;
  rgp->attach_and_clear_image("sky_envmap_image_mipped", attachment_final, vuk::Black<float>);

  auto blit_fut = vuk::blit_image({rgp, "sky_envmap_image+"}, {rgp, "sky_envmap_image_mipped"});

  auto mip_fut = vuk::generate_mips(blit_fut, 0, attachment_final.level_count);

  rg->attach_in("sky_envmap_image_final", mip_fut);
}

void DefaultRenderPipeline::sky_view_lut_pass(const Shared<vuk::RenderGraph>& rg) {
  OX_SCOPED_ZONE;

  const auto attachment = vuk::ImageAttachment{.extent = vuk::Dimension3D::absolute(192, 104, 1),
                                               .format = vuk::Format::eR16G16B16A16Sfloat,
                                               .sample_count = vuk::SampleCountFlagBits::e1,
                                               .base_level = 0,
                                               .level_count = 1,
                                               .base_layer = 0,
                                               .layer_count = 1};

  rg->attach_and_clear_image("sky_view_lut", attachment, vuk::Black<float>);

  rg->add_pass({.name = "sky_view_pass",
                .resources =
                  {
                    "sky_view_lut"_image >> vuk::eColorRW,
                    "sky_transmittance_lut+"_image >> vuk::eFragmentSampled,
                    "sky_multiscatter_lut+"_image >> vuk::eFragmentSampled,
                  },
                .execute = [this](vuk::CommandBuffer& command_buffer) {
    bind_camera_buffer(command_buffer);

    command_buffer.bind_persistent(0, *descriptor_set_00)
      .set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
      .set_viewport(0, vuk::Rect2D::framebuffer())
      .set_scissor(0, vuk::Rect2D::framebuffer())
      .broadcast_color_blend(vuk::BlendPreset::eOff)
      .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
      .bind_graphics_pipeline("sky_view_pipeline")
      .draw(3, 1, 0, 0);
  }});
}

void DefaultRenderPipeline::debug_pass(const Shared<vuk::RenderGraph>& rg,
                                       const vuk::Name dst,
                                       const char* depth,
                                       vuk::Allocator& frame_allocator) const {
  OX_SCOPED_ZONE;
  struct DebugPassData {
    Mat4 vp = {};
    Mat4 model = {};
    Vec4 color = {};
  };

  const auto* camera = current_camera;
  std::vector<DebugPassData> buffers = {};

  DebugPassData data = {.vp = camera->get_projection_matrix() * camera->get_view_matrix(), .model = glm::identity<Mat4>(), .color = Vec4(0, 1, 0, 1)};

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

  rg->add_pass({.name = "debug_shapes_pass",
                .resources = {vuk::Resource(dst, vuk::Resource::Type::eImage, vuk::eColorWrite, dst.append("+")),
                              vuk::Resource(depth, vuk::Resource::Type::eImage, vuk::eDepthStencilRead)},
                .execute =
                  [this, buffer, index_count, index_count_dt, vertex_buffer, vertex_buffer_dt, index_buffer](vuk::CommandBuffer& command_buffer) {
    const auto vertex_layout = vuk::Packed{vuk::Format::eR32G32B32A32Sfloat,
                                           vuk::Format::eR32G32B32A32Sfloat,
                                           vuk::Ignore{sizeof(Vertex) - (sizeof(Vec4) + sizeof(Vec4))}};

    // not depth tested
    command_buffer.bind_graphics_pipeline("unlit_pipeline")
      .set_depth_stencil(vuk::PipelineDepthStencilStateCreateInfo{
        .depthTestEnable = false,
        .depthWriteEnable = false,
        .depthCompareOp = vuk::CompareOp::eGreaterOrEqual,
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

    command_buffer.draw_indexed(index_count, 1, 0, 0, 0);

    command_buffer.bind_graphics_pipeline("unlit_pipeline")
      .set_depth_stencil(vuk::PipelineDepthStencilStateCreateInfo{
        .depthTestEnable = true,
        .depthWriteEnable = false,
        .depthCompareOp = vuk::CompareOp::eGreaterOrEqual,
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

    command_buffer.draw_indexed(index_count_dt, 1, 0, 0, 0);
  }});

  DebugRenderer::reset(true);
}
} // namespace ox
