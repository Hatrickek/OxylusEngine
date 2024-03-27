#include "Renderer.hpp"

#include <future>
#include <vuk/Partials.hpp>

#include "Assets/AssetManager.hpp"
#include "Core/LayerStack.hpp"
#include "Render/DebugRenderer.hpp"
#include "Render/DefaultRenderPipeline.h"
#include "Render/Mesh.h"
#include "Render/Window.h"
#include "Thread/TaskScheduler.hpp"
#include "UI/ImGuiLayer.hpp"
#include "Utils/Profiler.hpp"
#include "Vulkan/VkContext.hpp"

namespace ox {
Renderer::RendererContext Renderer::renderer_context;
RendererConfig Renderer::renderer_config;

void Renderer::init() {
  OX_SCOPED_ZONE;

  renderer_context.compiler = create_shared<vuk::Compiler>();

  Texture::create_white_texture();
  DebugRenderer::init();
}

void Renderer::deinit() {
  OX_SCOPED_ZONE;
  DebugRenderer::release();
}

void Renderer::draw(VkContext* vkctx, ImGuiLayer* imgui_layer, LayerStack& layer_stack) {
  OX_SCOPED_ZONE;

  const auto rp = renderer_context.render_pipeline;

  // consume renderer related cvars
  if (RendererCVar::cvar_reload_render_pipeline.get()) {
    rp->init(*VkContext::get()->superframe_allocator);
    RendererCVar::cvar_reload_render_pipeline.toggle();
  }

  // TODO:
  // const auto set_present_mode = RendererCVar::cvar_vsync.get() ? vuk::PresentModeKHR::eFifo : vuk::PresentModeKHR::eImmediate;
  // if (vkctx->present_mode != set_present_mode) {
  // vkctx->rebuild_swapchain(set_present_mode);
  // return;
  //}

  auto& frame_resource = vkctx->superframe_resource->get_next_frame();
  vkctx->context->next_frame();

  vuk::Allocator frame_allocator(frame_resource);

  auto imported_swapchain = vuk::declare_swapchain(*vkctx->swapchain);
  auto swapchain_image = vuk::acquire_next_image("swp_img", std::move(imported_swapchain));

  vuk::ProfilingCallbacks cbs = vkctx->tracy_profiler->setup_vuk_callback();

  vuk::Value<vuk::ImageAttachment> cleared_image = vuk::clear_image(std::move(swapchain_image), vuk::ClearColor{0.3f, 0.5f, 0.3f, 1.0f});

  auto extent = rp->is_swapchain_attached() ? vkctx->swapchain->images[vkctx->current_frame].extent : rp->get_extent();
  renderer_context.viewport_size = extent;
  renderer_context.viewport_offset = rp->get_viewport_offset();

  if (rp->is_swapchain_attached()) {
    vuk::Value<vuk::ImageAttachment> result = *rp->on_render(frame_allocator, cleared_image, extent);

    imgui_layer->begin();
    for (const auto& layer : layer_stack)
      layer->on_imgui_render();
    for (auto& [_, system] : App::get_system_registry())
      system->imgui_update();
    imgui_layer->end();

    result = imgui_layer->render_draw_data(frame_allocator, result);

    auto entire_thing = vuk::enqueue_presentation(std::move(result));
    entire_thing.wait(frame_allocator, *renderer_context.compiler, {.callbacks = cbs});
  } else {
    auto att = vuk::ImageAttachment{.extent = extent,
                                    .format = vkctx->swapchain->images[vkctx->current_frame].format,
                                    .sample_count = vuk::Samples::e1,
                                    .level_count = 1,
                                    .layer_count = 1};
    auto viewport_img = vuk::clear_image(vuk::declare_ia("viewport_img", att), vuk::Black<float>);

    // TODO: handle this
    vuk::Value<vuk::ImageAttachment> viewport_result = *rp->on_render(frame_allocator, viewport_img, extent); 

    imgui_layer->begin();
    for (const auto& layer : layer_stack)
      layer->on_imgui_render();
    for (auto& [_, system] : App::get_system_registry())
      system->imgui_update();
    imgui_layer->end();

    vuk::Value<vuk::ImageAttachment> result = imgui_layer->render_draw_data(frame_allocator, result);

    auto entire_thing = vuk::enqueue_presentation(std::move(result));
    entire_thing.wait(frame_allocator, *renderer_context.compiler, {.callbacks = cbs});
  }

  vkctx->current_frame = (vkctx->current_frame + 1) % vkctx->num_inflight_frames;
  vkctx->num_frames = vkctx->context->get_frame_count();
}
} // namespace ox
