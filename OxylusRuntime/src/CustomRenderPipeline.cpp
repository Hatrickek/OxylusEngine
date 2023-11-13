#include "CustomRenderPipeline.h"

#include <vuk/CommandBuffer.hpp>
#include <vuk/Partials.hpp>
#include <vuk/Pipeline.hpp>
#include <vuk/RenderGraph.hpp>

#include "Render/Vulkan/Renderer.h"
#include "Render/Vulkan/VukUtils.h"

#include "Utils/FileUtils.h"

namespace Oxylus {
void CustomRenderPipeline::init(vuk::Allocator& allocator) {
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileUtils::read_shader_file("Unlit.vert"), "Unlit.vert");
    pci.add_glsl(FileUtils::read_shader_file("Unlit.frag"), "Unlit.frag");
    allocator.get_context().create_named_pipeline("unlit_pipeline", pci);
  }
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_glsl(FileUtils::read_shader_file("FullscreenPass.vert"), "FullscreenPass.vert");
    pci.add_glsl(FileUtils::read_shader_file("SinglePassGaussianBlur.frag"), "SinglePassGaussianBlur.frag");
    allocator.get_context().create_named_pipeline("post_pipeline", pci);
  }
}

void CustomRenderPipeline::shutdown() {}

Scope<vuk::Future> CustomRenderPipeline::on_render(vuk::Allocator& frame_allocator, const vuk::Future& target, vuk::Dimension3D dim) {
  const auto rg = create_ref<vuk::RenderGraph>("custom_render_pipeline");

  rg->attach_in("final_image", std::move(target));

  struct UnlitPassData {
    Mat4 vp = {};
    Mat4 model = {};
    Vec4 color = {};
  };

  std::vector<UnlitPassData> pass_data = {};
  pass_data.reserve(draw_list.size());

  const std::vector color_list = {
    Vec4(1, 0, 0, 1),
    Vec4(1, 1, 0, 1),
    Vec4(1, 1, 1, 1),
    Vec4(0, 0, 1, 1),
    Vec4(0, 1, 1, 1),
    Vec4(1, 0, 1, 1),
  };

  uint32_t color_index = 0;
  for (const auto& mesh : draw_list) {
    UnlitPassData data = {
      .vp = current_camera->get_projection_matrix_flipped() * current_camera->get_view_matrix(),
      .model = mesh.transform,
      .color = color_list[color_index]
    };

    pass_data.emplace_back(data);

    color_index = (color_index + 1) % color_list.size();
  }

  auto [buff, buff_fut] = create_buffer(frame_allocator, vuk::MemoryUsage::eCPUtoGPU, vuk::DomainFlagBits::eTransferOnGraphics, std::span(pass_data));
  auto& buffer = *buff;

  rg->attach_and_clear_image("unlit_image", {.format = vuk::Format::eR8G8B8A8Unorm, .sample_count = vuk::SampleCountFlagBits::e1}, vuk::Black<float>);
  rg->attach_and_clear_image("depth_image", {.format = vuk::Format::eD32Sfloat, .sample_count = vuk::SampleCountFlagBits::e1}, vuk::DepthOne);
  rg->inference_rule("unlit_image", vuk::same_shape_as("final_image"));
  rg->inference_rule("depth_image", vuk::same_shape_as("final_image"));

  // Simple unlit mesh rendering
  rg->add_pass({
    .name = "geomerty_pass",
    .resources = {
      "unlit_image"_image >> vuk::eColorRW >> "unlit_output",
      "depth_image"_image >> vuk::eDepthStencilRW,
    },
    .execute = [this, buffer](vuk::CommandBuffer& command_buffer) {
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
                    .bind_buffer(0, 0, buffer)
                    .bind_graphics_pipeline("unlit_pipeline");

      for (uint32_t i = 0; i < (uint32_t)draw_list.size(); i++) {
        draw_list[i].mesh_geometry->bind_vertex_buffer(command_buffer);
        draw_list[i].mesh_geometry->bind_index_buffer(command_buffer);
        for (const auto* node : draw_list[i].mesh_geometry->nodes) {
          const struct PushConst {
            uint32_t buffer_index;
          } pc = {i};
          command_buffer.push_constants(vuk::ShaderStageFlagBits::eVertex, 0, pc);
          Renderer::render_node(node, command_buffer);
        }
      }

      draw_list.clear();
    }
  });

  // Simple gaussian blurring
  rg->add_pass({
    .name = "gaussian_pass",
    .resources = {
      "final_image"_image >> vuk::eColorRW >> "final_output",
      "unlit_output"_image >> vuk::eFragmentSampled,
    },
    .execute = [this](vuk::CommandBuffer& command_buffer) {
      command_buffer.set_dynamic_state(vuk::DynamicStateFlagBits::eScissor | vuk::DynamicStateFlagBits::eViewport)
                    .set_viewport(0, vuk::Rect2D::framebuffer())
                    .set_scissor(0, vuk::Rect2D::framebuffer())
                    .broadcast_color_blend(vuk::BlendPreset::eOff)
                    .set_rasterization({.cullMode = vuk::CullModeFlagBits::eNone})
                    .bind_image(0, 0, "unlit_output")
                    .bind_sampler(0, 0, vuk::LinearSamplerClamped)
                    .bind_graphics_pipeline("post_pipeline")
                    .draw(3, 1, 0, 0);
    }
  });

  return create_scope<vuk::Future>(std::move(rg), "final_output");
}

void CustomRenderPipeline::on_register_render_object(const MeshData& render_object) {
  draw_list.emplace_back(render_object);
}

void CustomRenderPipeline::on_register_camera(Camera* camera) {
  current_camera = camera;
}
}
