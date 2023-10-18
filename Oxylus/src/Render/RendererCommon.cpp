#include "RendererCommon.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include <vuk/CommandBuffer.hpp>
#include <vuk/Partials.hpp>
#include <vuk/Pipeline.hpp>
#include <vuk/RenderGraph.hpp>

#include "Mesh.h"

#include "Assets/AssetManager.h"

#include "Core/Resources.h"
#include "Core/Types.h"

#include "Utils/FileUtils.h"
#include "Utils/Log.h"

#include "Vulkan/VukUtils.h"
#include "Vulkan/VulkanContext.h"
#include "Vulkan/VulkanRenderer.h"

namespace Oxylus {
std::pair<vuk::Unique<vuk::Image>, vuk::Future> RendererCommon::generate_cubemap_from_equirectangular(const vuk::Texture& cubemap) {
  auto& allocator = *VulkanContext::get()->superframe_allocator;

  // blur the hdr texture
  Ref<vuk::RenderGraph> rg = create_ref<vuk::RenderGraph>("cubegen");
  rg->attach_image("hdr_image", vuk::ImageAttachment::from_texture(cubemap));
  rg->attach_and_clear_image("blurred_hdr_image", vuk::ImageAttachment{.sample_count = vuk::SampleCountFlagBits::e1, .view_type = vuk::ImageViewType::e2D}, vuk::Black<float>);
  rg->inference_rule("blurred_hdr_image", vuk::same_extent_as("hdr_image"));
  rg->inference_rule("blurred_hdr_image", vuk::same_format_as("hdr_image"));

  apply_blur(rg, "hdr_image", "blurred_hdr_image", "blurred_hdr_image_output");

  vuk::Unique<vuk::Image> output;

  constexpr auto size = 2048;

  const auto mip_count = static_cast<uint32_t>(log2f((float)std::max(size, size))) + 1;

  auto env_cubemap_ia = vuk::ImageAttachment{
    .image = *output,
    .image_flags = vuk::ImageCreateFlagBits::eCubeCompatible,
    .image_type = vuk::ImageType::e2D,
    .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment |
             vuk::ImageUsageFlagBits::eTransferSrc | vuk::ImageUsageFlagBits::eTransferDst,
    .extent = vuk::Dimension3D::absolute(size, size, 1),
    .format = vuk::Format::eR32G32B32A32Sfloat,
    .sample_count = vuk::Samples::e1,
    .view_type = vuk::ImageViewType::eCube,
    .base_level = 0,
    .level_count = mip_count,
    .base_layer = 0,
    .layer_count = 6
  };
  output = *allocate_image(allocator, env_cubemap_ia);
  env_cubemap_ia.image = *output;

  if (!allocator.get_context().is_pipeline_available("equirectangular_to_cubemap")) {
    vuk::PipelineBaseCreateInfo equirectangular_to_cubemap;
    equirectangular_to_cubemap.add_glsl(FileUtils::read_shader_file("Cubemap.vert"), "Cubemap.vert");
    equirectangular_to_cubemap.add_glsl(FileUtils::read_shader_file("EquirectangularToCubemap.frag"), "EquirectangularToCubemap.frag");
    allocator.get_context().create_named_pipeline("equirectangular_to_cubemap", equirectangular_to_cubemap);
  }

  const auto cube = AssetManager::get_mesh_asset(Resources::get_resources_path("Objects/cube.glb"));

  const auto capture_projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
  const Mat4 capture_views[] = {
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f)),
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(-1.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f)),
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f)),
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f)),
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), Vec3(0.0f, -1.0f, 0.0f)),
    lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f), Vec3(0.0f, -1.0f, 0.0f))
  };

  rg->attach_image("env_cubemap", env_cubemap_ia, vuk::Access::eNone);
  rg->add_pass({
    .resources = {
      "env_cubemap"_image >> vuk::eColorWrite,
      "blurred_hdr_image_output"_image >> vuk::eFragmentSampled
    },
    .execute = [=](vuk::CommandBuffer& cbuf) {
      cbuf.set_viewport(0, vuk::Rect2D::framebuffer())
          .set_scissor(0, vuk::Rect2D::framebuffer())
          .broadcast_color_blend(vuk::BlendPreset::eOff)
          .set_rasterization({})
          .bind_image(0, 2, "blurred_hdr_image_output")
          .bind_sampler(0, 2, vuk::LinearSamplerClamped)
          .bind_graphics_pipeline("equirectangular_to_cubemap");

      auto* projection = cbuf.map_scratch_buffer<glm::mat4>(0, 0);
      *projection = capture_projection;
      const auto view = cbuf.map_scratch_buffer<glm::mat4[6]>(0, 1);
      memcpy(view, capture_views, sizeof(capture_views));

      cube->bind_vertex_buffer(cbuf);
      cube->bind_index_buffer(cbuf);
      for (const auto& node : cube->nodes)
        for (const auto& primitive : node->primitives)
          cbuf.draw_indexed(primitive->index_count, 6, 0, 0, 0);
    }
  });

  return {
    std::move(output), transition(vuk::Future{std::move(rg), "env_cubemap+"}, vuk::eFragmentSampled)
  };
}

void RendererCommon::apply_blur(const std::shared_ptr<vuk::RenderGraph>& render_graph, vuk::Name src_attachment, const vuk::Name attachment_name, const vuk::Name attachment_name_output) {
  auto& allocator = *VulkanContext::get()->superframe_allocator;
  if (!allocator.get_context().is_pipeline_available("gaussian_blur_pipeline")) {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileUtils::read_shader_file("FullscreenPass.vert"), FileUtils::get_shader_path("FullscreenPass.vert"));
    pci.add_glsl(FileUtils::read_shader_file("SinglePassGaussianBlur.frag"), "SinglePassGaussianBlur.frag");
    allocator.get_context().create_named_pipeline("gaussian_blur_pipeline", pci);
  }

  render_graph->add_pass({
    .name = "blur",
    .resources = {
      vuk::Resource(attachment_name, vuk::Resource::Type::eImage, vuk::eColorRW, attachment_name_output),
      vuk::Resource(src_attachment, vuk::Resource::Type::eImage, vuk::eFragmentSampled),
    },
    .execute = [src_attachment](vuk::CommandBuffer& command_buffer) {
      command_buffer.bind_graphics_pipeline("gaussian_blur_pipeline")
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .broadcast_color_blend(vuk::BlendPreset::eOff)
                    .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
                    .bind_image(0, 0, src_attachment)
                    .bind_sampler(0, 0, vuk::LinearSamplerClamped)
                    .draw(3, 1, 0, 0);
    }
  });
}
}
