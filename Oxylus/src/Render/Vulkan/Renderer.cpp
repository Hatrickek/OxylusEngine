#include "Renderer.h"

#include <future>
#include <vuk/Partials.hpp>

#include "VulkanContext.h"
#include "Assets/AssetManager.h"
#include "Core/Systems/SystemManager.h"
#include "Render/Mesh.h"
#include "Render/Window.h"
#include "Utils/Profiler.h"
#include "Render/DebugRenderer.h"
#include "Render/DefaultRenderPipeline.h"

namespace Oxylus {
Renderer::RendererContext Renderer::renderer_context;
RendererConfig Renderer::renderer_config;

void Renderer::init() {
  // Save/Load renderer config
  if (!RendererConfig::get()->load_config("renderer.oxconfig"))
    RendererConfig::get()->save_config("renderer.oxconfig");

  TextureAsset::create_blank_texture();
  TextureAsset::create_white_texture();

  // Debug renderer
  DebugRenderer::init();
}

void Renderer::shutdown() {
  RendererConfig::get()->save_config("renderer.oxconfig");
  DebugRenderer::release();
}

void Renderer::draw(VulkanContext* context, ImGuiLayer* imgui_layer, LayerStack& layer_stack, const Ref<SystemManager>& system_manager) {
  OX_SCOPED_ZONE;
  imgui_layer->begin();

  auto frameAllocator = context->begin();
  const Ref<vuk::RenderGraph> rg = create_ref<vuk::RenderGraph>("runner");
  rg->attach_swapchain("_swp", context->swapchain);
  rg->clear_image("_swp", "final_image", vuk::ClearColor{0.0f, 0.0f, 0.0f, 1.0f});

  const auto rp = renderer_context.render_pipeline;

  vuk::Future fut = {};

  // Render it directly to swapchain
  if (rp->is_swapchain_attached()) {
    renderer_context.viewport_size.x = context->swapchain->extent.width;
    renderer_context.viewport_size.y = context->swapchain->extent.height;

    const auto dim = vuk::Dimension3D::absolute(context->swapchain->extent);

    fut = *rp->on_render(frameAllocator, vuk::Future{rg, "final_image"}, dim);

    for (const auto& layer : layer_stack)
      layer->on_imgui_render();
    system_manager->on_imgui_render();
    imgui_layer->end();

    fut = imgui_layer->render_draw_data(frameAllocator, fut, ImGui::GetDrawData());
  }
  // Render it into a separate image with given dimension
  else {
    const auto rgx = create_ref<vuk::RenderGraph>(rp->get_name().c_str());

    const auto& dim = rp->get_dimension();

    OX_CORE_ASSERT(dim.extent.width > 0)
    OX_CORE_ASSERT(dim.extent.height > 0)

    renderer_context.viewport_size.x = dim.extent.width;
    renderer_context.viewport_size.y = dim.extent.height;

    rgx->attach_and_clear_image("_img",
      vuk::ImageAttachment{
        .extent = dim,
        .format = context->swapchain->format,
        .sample_count = vuk::Samples::e1,
        .level_count = 1,
        .layer_count = 1
      },
      vuk::ClearColor(0.0f, 0.0f, 0.0f, 1.f)
    );

    auto rpFut = *rp->on_render(frameAllocator, vuk::Future{rgx, "_img"}, dim);
    const auto attachmentNameOut = rpFut.get_bound_name().name;

    auto rpRg = rpFut.get_render_graph();

    vuk::Compiler compiler;
    compiler.compile({&rpRg, 1}, {});

    rg->attach_in(attachmentNameOut, std::move(rpFut));

    auto si = create_ref<vuk::SampledImage>(make_sampled_image(vuk::NameReference{rg.get(), vuk::QualifiedName({}, attachmentNameOut)}, {}));
    rp->set_final_image(si);
    rp->set_frame_render_graph(rg);
    rp->set_frame_allocator(&frameAllocator);

    for (const auto& layer : layer_stack)
      layer->on_imgui_render();

    system_manager->on_imgui_render();

    imgui_layer->end();

    fut = imgui_layer->render_draw_data(frameAllocator, vuk::Future{rg, "final_image"}, ImGui::GetDrawData());
  }

  context->end(fut, frameAllocator);
}

void Renderer::render_node(const Mesh::Node* node, vuk::CommandBuffer& command_buffer, const std::function<bool(Mesh::Primitive* prim, Mesh::MeshData* mesh_data)>& per_mesh_func) {
  if (node->mesh_data) {
    for (const auto& part : node->mesh_data->primitives) {
      if (!per_mesh_func(part, node->mesh_data))
        continue;

      command_buffer.draw_indexed(part->index_count, 1, part->first_index, 0, 0);
    }
  }
  for (const auto& child : node->children)
    render_node(child, command_buffer, per_mesh_func);
}

void Renderer::render_mesh(const MeshData& mesh,
                                 vuk::CommandBuffer& command_buffer,
                                 const std::function<bool(Mesh::Primitive* prim, Mesh::MeshData* mesh_data)>& per_mesh_func) {
  mesh.mesh_geometry->bind_vertex_buffer(command_buffer);
  mesh.mesh_geometry->bind_index_buffer(command_buffer);
  render_node(mesh.mesh_geometry->linear_nodes[mesh.submesh_index], command_buffer, per_mesh_func);
}
}
