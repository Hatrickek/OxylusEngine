#include "Renderer.hpp"

#include <future>
#include <vuk/Partials.hpp>

#include "Thread/TaskScheduler.hpp"
#include "UI/ImGuiLayer.hpp"
#include "Utils/Profiler.hpp"
#include "Assets/AssetManager.hpp"
#include "Core/LayerStack.hpp"
#include "Render/DebugRenderer.hpp"
#include "Render/DefaultRenderPipeline.h"
#include "Render/Mesh.h"
#include "Render/Window.h"
#include "Vulkan/VkContext.hpp"

namespace ox {
Renderer::RendererContext Renderer::renderer_context;
RendererConfig Renderer::renderer_config;

void Renderer::init() {
  OX_SCOPED_ZONE;

  TextureAsset::create_white_texture();
  DebugRenderer::init();
}

void Renderer::deinit() {
  OX_SCOPED_ZONE;
  DebugRenderer::release();
}

void Renderer::draw(VkContext* context, ImGuiLayer* imgui_layer, LayerStack& layer_stack) {
  OX_SCOPED_ZONE;

  const auto rp = renderer_context.render_pipeline;

  // consume renderer related cvars
  if (RendererCVar::cvar_reload_render_pipeline.get()) {
    rp->init(*VkContext::get()->superframe_allocator);
    RendererCVar::cvar_reload_render_pipeline.toggle();
  }

  const auto set_present_mode = RendererCVar::cvar_vsync.get() ? vuk::PresentModeKHR::eFifo : vuk::PresentModeKHR::eImmediate;
  if (context->present_mode != set_present_mode) {
    context->rebuild_swapchain(set_present_mode);
    return;
  }

  imgui_layer->begin();

  auto frame_allocator = context->begin();
  const Shared<vuk::RenderGraph> rg = create_shared<vuk::RenderGraph>("runner");
  rg->attach_swapchain("_swp", context->swapchain);
  rg->clear_image("_swp", "target_image", vuk::ClearColor{0.0f, 0.0f, 0.0f, 1.0f});

  vuk::Future fut = {};

  // Render it directly to swapchain
  if (rp->is_swapchain_attached()) {
    renderer_context.viewport_size.x = context->swapchain->extent.width;
    renderer_context.viewport_size.y = context->swapchain->extent.height;

    const auto dim = vuk::Dimension3D::absolute(context->swapchain->extent);

    const auto cleared_image = vuk::Future{std::move(rg), "target_image"};
    fut = *rp->on_render(frame_allocator, cleared_image, dim);

    for (const auto& layer : layer_stack)
      layer->on_imgui_render();
    for (auto& [_, system] : App::get_system_registry())
      system->imgui_update();
    imgui_layer->end();

    fut = imgui_layer->render_draw_data(frame_allocator, fut, ImGui::GetDrawData());
  }
  // Render it into a separate image with given dimension
  else {
    const auto rgx = create_shared<vuk::RenderGraph>(rp->get_name().c_str());

    auto dim = rp->get_dimension();

    OX_CHECK_GT(dim.extent.width, 0);
    OX_CHECK_GT(dim.extent.height, 0);

    // recover if the size is somehow 0
    if (dim.extent.width <= 0)
      dim.extent.width = 10;
    if (dim.extent.height <= 0)
      dim.extent.height = 10;

    renderer_context.viewport_size.x = dim.extent.width;
    renderer_context.viewport_size.y = dim.extent.height;

    renderer_context.viewport_offset = rp->get_viewport_offset();

    rgx->attach_and_clear_image(
      "_img",
      vuk::ImageAttachment{.extent = dim, .format = context->swapchain->format, .sample_count = vuk::Samples::e1, .level_count = 1, .layer_count = 1},
      vuk::ClearColor(0.0f, 0.0f, 0.0f, 1.f));

    const auto rp_fut = rp->on_render(frame_allocator, vuk::Future{std::move(rgx), "_img"}, dim);
    const auto attachment_name_out = rp_fut->get_bound_name().name;

    auto rp_rg = rp_fut->get_render_graph();

    vuk::Compiler compiler;
    compiler.compile({&rp_rg, 1}, {});

    rg->attach_in(attachment_name_out, std::move(*rp_fut));

    auto si = create_shared<vuk::SampledImage>(make_sampled_image(vuk::NameReference{rg.get(), vuk::QualifiedName({}, attachment_name_out)}, {}));
    rp->set_final_image(si);
    rp->set_frame_render_graph(rp_rg);
    rp->set_frame_allocator(&frame_allocator);
    rp->set_final_attachment_name(attachment_name_out);
    rp->set_compiler(&compiler);

    rg->attach_in(rp->get_rg_futures());
    rp->get_rg_futures().clear();

    for (const auto& layer : layer_stack)
      layer->on_imgui_render();

    for (auto& [_, system] : App::get_system_registry())
      system->imgui_update();

    imgui_layer->end();

    fut = imgui_layer->render_draw_data(frame_allocator, vuk::Future{std::move(rg), "target_image"}, ImGui::GetDrawData());
  }

  context->end(fut, frame_allocator);
}
} // namespace ox
