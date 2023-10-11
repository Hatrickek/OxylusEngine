#include "Prefilter.h"

#include <glm/gtx/quaternion.hpp>
#include <vuk/CommandBuffer.hpp>
#include <vuk/Partials.hpp>
#include <vuk/Pipeline.hpp>

#include "Render/Mesh.h"
#include "Render/Vulkan/VulkanContext.h"
#include "Utils/FileUtils.h"

#define M_PI       3.14159265358979323846

namespace Oxylus {
std::pair<vuk::Unique<vuk::Image>, vuk::Future> Prefilter::generate_brdflut() {
  constexpr uint32_t dim = 512;

  const auto vkContext = VulkanContext::get();

  vuk::PipelineBaseCreateInfo pci;
  pci.add_glsl(FileUtils::read_shader_file("GenBrdfLut.vert"), "GenBrdfLut.vert");
  pci.add_glsl(FileUtils::read_shader_file("GenBrdfLut.frag"), "GenBrdfLut.frag");
  vkContext->context->create_named_pipeline("BRDFLUTPipeline", pci);

  vuk::Unique<vuk::Image> brdfImage;

  vuk::ImageAttachment attachment = {
    .image = *brdfImage,
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

  brdfImage = *allocate_image(*vkContext->superframe_allocator, attachment);
  attachment.image = *brdfImage;

  vuk::RenderGraph rg("BRDFLUT");
  rg.attach_image("BRDF-Output", attachment);
  rg.add_pass({
    "BRDFLUTPass",
    vuk::DomainFlagBits::eGraphicsOnGraphics,
    {"BRDF-Output"_image >> vuk::eColorWrite},
    [](vuk::CommandBuffer& commandBuffer) {
      commandBuffer.broadcast_color_blend({})
                   .set_rasterization({})
                   .set_depth_stencil({})
                   .set_viewport(0, vuk::Rect2D::framebuffer())
                   .set_scissor(0, vuk::Rect2D::framebuffer())
                   .bind_graphics_pipeline("BRDFLUTPipeline")
                   .draw(3, 1, 0, 0);
    }
  });

  return {std::move(brdfImage), transition(vuk::Future{std::make_unique<vuk::RenderGraph>(std::move(rg)), "BRDF-Output+"}, vuk::eFragmentSampled)};
}

std::pair<vuk::Unique<vuk::Image>, vuk::Future> Prefilter::generate_irradiance_cube(const Ref<Mesh>& skybox,
                                                                                  const Ref<TextureAsset>& cubemap) {
  const auto vkContext = VulkanContext::get();
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
    float deltaPhi;
    float deltaTheta;
  } pushBlock = {};

  pushBlock.deltaPhi = (2.0f * static_cast<float>(M_PI)) / 180.0f;
  pushBlock.deltaTheta = (0.5f * static_cast<float>(M_PI)) / 64.0f;


  constexpr auto pipelineName = "IrradiancePipeline";

  //if (!vkContext->context->get_named_pipeline(pipelineName)) {
  vuk::PipelineBaseCreateInfo pci;
  pci.add_glsl(FileUtils::read_shader_file("Cubemap.vert"), "Cubemap.vert");
  pci.add_glsl(FileUtils::read_shader_file("IrradianceCube.frag"), "IrradianceCube.frag");
  vkContext->context->create_named_pipeline(pipelineName, pci);
  //}

  vuk::Unique<vuk::Image> irradianceImage;

  vuk::ImageAttachment irradianceIA = {
    .image = *irradianceImage,
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
  irradianceImage = *allocate_image(*vkContext->superframe_allocator, irradianceIA);
  irradianceIA.image = *irradianceImage;

  vuk::RenderGraph rg("Irradiance");
  rg.attach_image("IrradianceCube", irradianceIA);
  rg.add_pass({
    "IrradiancePass",
    vuk::DomainFlagBits::eGraphicsOnGraphics,
    {"IrradianceCube"_image >> vuk::eColorWrite},
    [=](vuk::CommandBuffer& commandBuffer) {
      commandBuffer.broadcast_color_blend(vuk::BlendPreset::eOff)
                   .set_rasterization({})
                   .set_depth_stencil({})
                   .set_viewport(0, vuk::Rect2D::framebuffer())
                   .set_scissor(0, vuk::Rect2D::framebuffer())
                   .bind_graphics_pipeline("IrradiancePipeline")
                   .bind_sampler(1, 0, {.magFilter = vuk::Filter::eLinear, .minFilter = vuk::Filter::eLinear})
                   .bind_image(1, 0, cubemap->as_attachment())
                   .push_constants(vuk::ShaderStageFlagBits::eFragment, 0, pushBlock);

      auto* projection = commandBuffer.map_scratch_buffer<glm::mat4>(0, 0);
      *projection = capture_projection;
      const auto view = commandBuffer.map_scratch_buffer<glm::mat4[6]>(0, 1);
      memcpy(view, capture_views, sizeof(capture_views));

      skybox->bind_vertex_buffer(commandBuffer);
      skybox->bind_index_buffer(commandBuffer);
      for (const auto& node : skybox->nodes) {
        for (const auto& primitive : node->primitives)
          commandBuffer.draw_indexed(primitive->index_count, 6, 0, 0, 0);
      }
    }
  });

  return {std::move(irradianceImage), transition(vuk::Future{std::make_unique<vuk::RenderGraph>(std::move(rg)), "IrradianceCube+"}, vuk::eFragmentSampled)};
}

std::pair<vuk::Unique<vuk::Image>, vuk::Future> Prefilter::generate_prefiltered_cube(const Ref<Mesh>& skybox,
                                                                                   const Ref<TextureAsset>& cubemap) {
  const auto vkContext = VulkanContext::get();
  constexpr int32_t dim = 512;

  constexpr auto pipelineName = "PrefilterPipeline";

  //if (!vkContext->context->get_named_pipeline(pipelineName)) {
  vuk::PipelineBaseCreateInfo pci;
  pci.add_glsl(FileUtils::read_shader_file("Cubemap.vert"), "Cubemap.vert");
  pci.add_glsl(FileUtils::read_shader_file("PrefilterEnvMap.frag"), "PrefilterEnvMap.frag");
  vkContext->context->create_named_pipeline(pipelineName, pci);
  //}

  struct PushBlock {
    float roughness = {};
    uint32_t numSamples = 32u;
  } pushBlock;

  const auto capture_projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
  const Mat4 capture_views[] = {
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f)),
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(-1.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f)),
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f)),
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f)),
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), Vec3(0.0f, -1.0f, 0.0f)),
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f), Vec3(0.0f, -1.0f, 0.0f))
  };

  vuk::Unique<vuk::Image> prefilterImage;

  vuk::ImageAttachment prefilterIA = {
    .image = *prefilterImage,
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
  prefilterImage = *allocate_image(*vkContext->superframe_allocator, prefilterIA);
  prefilterIA.image = *prefilterImage;

  vuk::RenderGraph rg("Prefilter");
  rg.attach_image("PrefilterCube", prefilterIA);
  rg.add_pass({
    "PrefilterPass",
    vuk::DomainFlagBits::eGraphicsOnGraphics,
    {"PrefilterCube"_image >> vuk::eColorWrite},
    [=](vuk::CommandBuffer& commandBuffer) {
      commandBuffer.broadcast_color_blend(vuk::BlendPreset::eOff)
                   .set_rasterization({})
                   .set_depth_stencil({})
                   .set_viewport(0, vuk::Rect2D::framebuffer())
                   .set_scissor(0, vuk::Rect2D::framebuffer())
                   .bind_graphics_pipeline("PrefilterPipeline")
                   .bind_sampler(1, 0, {.magFilter = vuk::Filter::eLinear, .minFilter = vuk::Filter::eLinear})
                   .bind_image(1, 0, cubemap->as_attachment())
                   .push_constants(vuk::ShaderStageFlagBits::eFragment, 0, pushBlock);

      auto* projection = commandBuffer.map_scratch_buffer<glm::mat4>(0, 0);
      *projection = capture_projection;
      const auto view = commandBuffer.map_scratch_buffer<glm::mat4[6]>(0, 1);
      memcpy(view, capture_views, sizeof(capture_views));

      skybox->bind_vertex_buffer(commandBuffer);
      skybox->bind_index_buffer(commandBuffer);
      for (const auto& node : skybox->nodes) {
        for (const auto& primitive : node->primitives)
          commandBuffer.draw_indexed(primitive->index_count, 6, 0, 0, 0);
      }
    }
  });

  return {std::move(prefilterImage), transition(vuk::Future{std::make_unique<vuk::RenderGraph>(std::move(rg)), "PrefilterCube+"}, vuk::eFragmentSampled)};
}
}
