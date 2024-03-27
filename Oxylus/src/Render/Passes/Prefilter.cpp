#include "Prefilter.hpp"

#include <glm/gtx/quaternion.hpp>

#include <vuk/CommandBuffer.hpp>
#include <vuk/Partials.hpp>
#include <vuk/Pipeline.hpp>

#include "Core/FileSystem.hpp"
#include "Render/Utils/VukCommon.hpp"
#include "Render/Vulkan/VkContext.hpp"
#include "Render/Mesh.h"

#define M_PI       3.14159265358979323846

namespace ox {
std::pair<vuk::Texture, vuk::Future> Prefilter::generate_brdflut() {
  constexpr uint32_t dim = 512;

  const auto vk_context = VkContext::get();

  constexpr auto pipeline_name = "BRDFLUTPipeline";

  if (!vk_context->context->is_pipeline_available(pipeline_name)) {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileSystem::read_shader_file("GenBrdfLut.vert"), "GenBrdfLut.vert");
    pci.add_glsl(FileSystem::read_shader_file("GenBrdfLut.frag"), "GenBrdfLut.frag");
    vk_context->context->create_named_pipeline(pipeline_name, pci);
  }

  vuk::ImageAttachment attachment = {
    .image_type = vuk::ImageType::e2D,
    .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
    .extent = vuk::Dimension3D::absolute(dim, dim, 1),
    .format = vuk::Format::eR32G32B32A32Sfloat,
    .sample_count = vuk::Samples::e1,
    .view_type = vuk::ImageViewType::e2D,
    .base_level = 0,
    .level_count = 1,
    .base_layer = 0,
    .layer_count = 1
  };

  vuk::Texture brdf_texture = create_texture(*vk_context->superframe_allocator, attachment);

  attachment.image = *brdf_texture.image;

  vuk::RenderGraph rg("BRDFLUT");
  rg.attach_image("BRDF-Output", attachment);
  rg.add_pass({
    .name = "BRDFLUTPass",
    .resources = {"BRDF-Output"_image >> vuk::eColorWrite},
    .execute = [](vuk::CommandBuffer& command_buffer) {
      command_buffer.broadcast_color_blend({})
                    .set_rasterization({})
                    .set_depth_stencil({})
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .bind_graphics_pipeline("BRDFLUTPipeline")
                    .draw(3, 1, 0, 0);
    }
  });

  return {std::move(brdf_texture), transition(vuk::Future{std::make_unique<vuk::RenderGraph>(std::move(rg)), "BRDF-Output+"}, vuk::eFragmentSampled)};
}

std::pair<vuk::Texture, vuk::Future> Prefilter::generate_irradiance_cube(const Shared<Mesh>& skybox,
                                                                        const Shared<Texture>& cubemap) {
  const auto vk_context = VkContext::get();
  constexpr int32_t dim = 64;

  const auto capture_projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
  const Mat4 capture_views[] = {
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f)),
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(-1.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f)),
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f)),
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f)),
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), Vec3(0.0f, -1.0f, 0.0f)),
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f), Vec3(0.0f, -1.0f, 0.0f))
  };

  struct PushBlock {
    float delta_phi;
    float delta_theta;
  } push_block = {};

  push_block.delta_phi = 2.0f * static_cast<float>(M_PI) / 180.0f;
  push_block.delta_theta = 0.5f * static_cast<float>(M_PI) / 64.0f;

  constexpr auto pipeline_name = "IrradiancePipeline";

  if (!vk_context->context->is_pipeline_available(pipeline_name)) {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileSystem::read_shader_file("Cubemap.vert"), "Cubemap.vert");
    pci.add_glsl(FileSystem::read_shader_file("IrradianceCube.frag"), "IrradianceCube.frag");
    vk_context->context->create_named_pipeline(pipeline_name, pci);
  }

  vuk::ImageAttachment irradiance_ia = {
    .image_flags = vuk::ImageCreateFlagBits::eCubeCompatible,
    .image_type = vuk::ImageType::e2D,
    .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
    .extent = vuk::Dimension3D::absolute(dim, dim, 1),
    .format = vuk::Format::eR32G32B32A32Sfloat,
    .sample_count = vuk::Samples::e1,
    .view_type = vuk::ImageViewType::eCube,
    .base_level = 0,
    .level_count = 1,
    .base_layer = 0,
    .layer_count = 6
  };

  vuk::Texture irradiance_texture = create_texture(*vk_context->superframe_allocator, irradiance_ia);
  irradiance_ia.image = *irradiance_texture.image;

  vuk::RenderGraph rg("Irradiance");
  rg.attach_image("IrradianceCube", irradiance_ia);
  rg.add_pass({
    .name = "IrradiancePass",
    .resources = {"IrradianceCube"_image >> vuk::eColorWrite},
    .execute = [=](vuk::CommandBuffer& command_buffer) {
      command_buffer.broadcast_color_blend(vuk::BlendPreset::eOff)
                    .set_rasterization({})
                    .set_depth_stencil({})
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .bind_graphics_pipeline("IrradiancePipeline")
                    .bind_sampler(1, 0, {.magFilter = vuk::Filter::eLinear, .minFilter = vuk::Filter::eLinear})
                    .bind_image(1, 0, cubemap->as_attachment())
                    .push_constants(vuk::ShaderStageFlagBits::eFragment, 0, push_block);

      auto* projection = command_buffer.map_scratch_buffer<glm::mat4>(0, 0);
      *projection = capture_projection;
      const auto view = command_buffer.map_scratch_buffer<glm::mat4[6]>(0, 1);
      memcpy(view, capture_views, sizeof(capture_views));

      skybox->bind_vertex_buffer(command_buffer);
      skybox->bind_index_buffer(command_buffer);
      command_buffer.draw_indexed(skybox->index_count, 6, 0, 0, 0);
    }
  });

  return {std::move(irradiance_texture), transition(vuk::Future{std::make_unique<vuk::RenderGraph>(std::move(rg)), "IrradianceCube+"}, vuk::eFragmentSampled)};
}

std::pair<vuk::Texture, vuk::Future> Prefilter::generate_prefiltered_cube(const Shared<Mesh>& skybox,
                                                                                     const Shared<Texture>& cubemap) {
  const auto vk_context = VkContext::get();
  constexpr int32_t dim = 512;

  constexpr auto pipeline_name = "PrefilterPipeline";

  if (!vk_context->context->is_pipeline_available(pipeline_name)) {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileSystem::read_shader_file("Cubemap.vert"), "Cubemap.vert");
    pci.add_glsl(FileSystem::read_shader_file("PrefilterEnvMap.frag"), "PrefilterEnvMap.frag");
    vk_context->context->create_named_pipeline(pipeline_name, pci);
  }

  struct PushBlock {
    float roughness = {};
    uint32_t num_samples = 32u;
  } push_block;

  const auto capture_projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
  const Mat4 capture_views[] = {
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f)),
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(-1.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f)),
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f)),
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f)),
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), Vec3(0.0f, -1.0f, 0.0f)),
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f), Vec3(0.0f, -1.0f, 0.0f))
  };

  vuk::ImageAttachment prefilter_ia = {
    .image_flags = vuk::ImageCreateFlagBits::eCubeCompatible,
    .image_type = vuk::ImageType::e2D,
    .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
    .extent = vuk::Dimension3D::absolute(dim, dim, 1),
    .format = vuk::Format::eR32G32B32A32Sfloat,
    .sample_count = vuk::Samples::e1,
    .view_type = vuk::ImageViewType::eCube,
    .base_level = 0,
    .level_count = 1,
    .base_layer = 0,
    .layer_count = 6
  };

  vuk::Texture prefilter_texture = create_texture(*vk_context->superframe_allocator, prefilter_ia);
  prefilter_ia.image = *prefilter_texture.image;

  vuk::RenderGraph rg("Prefilter");
  rg.attach_image("PrefilterCube", prefilter_ia);
  rg.add_pass({
    .name = "PrefilterPass",
    .resources = {"PrefilterCube"_image >> vuk::eColorWrite},
    .execute = [=](vuk::CommandBuffer& command_buffer) {
      command_buffer.broadcast_color_blend(vuk::BlendPreset::eOff)
                    .set_rasterization({})
                    .set_depth_stencil({})
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .bind_graphics_pipeline("PrefilterPipeline")
                    .bind_sampler(1, 0, {.magFilter = vuk::Filter::eLinear, .minFilter = vuk::Filter::eLinear})
                    .bind_image(1, 0, cubemap->as_attachment())
                    .push_constants(vuk::ShaderStageFlagBits::eFragment, 0, push_block);

      auto* projection = command_buffer.map_scratch_buffer<glm::mat4>(0, 0);
      *projection = capture_projection;
      const auto view = command_buffer.map_scratch_buffer<glm::mat4[6]>(0, 1);
      memcpy(view, capture_views, sizeof(capture_views));

      skybox->bind_vertex_buffer(command_buffer);
      skybox->bind_index_buffer(command_buffer);
      command_buffer.draw_indexed(skybox->index_count, 6, 0, 0, 0);
    }
  });

  return {std::move(prefilter_texture), transition(vuk::Future{std::make_unique<vuk::RenderGraph>(std::move(rg)), "PrefilterCube+"}, vuk::eFragmentSampled)};
}
}
