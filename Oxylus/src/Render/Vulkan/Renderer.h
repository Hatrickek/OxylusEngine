#pragma once

#include "Scene/Components.h"

#include "Render/RendererConfig.h"
#include "Render/RenderPipeline.h"

namespace ox {
class DefaultRenderPipeline;
class LayerStack;
class ImGuiLayer;
class VkContext;

class Renderer {
public:
  static struct RendererContext {
    // TODO:
    // need to get rid of this and instead use the render pipeline from scenes directly.
    // currently this gets filled at scene creation 
    Shared<RenderPipeline> render_pipeline = nullptr;
    UVec2 viewport_size = {1600, 900};
    Vec2 viewport_offset = {};
  } renderer_context;

  static void init();
  static void deinit();

  static void draw(VkContext* context, ImGuiLayer* imgui_layer, LayerStack& layer_stack);

  static UVec2 get_viewport_size() { return renderer_context.viewport_size; }
  static Vec2 get_viewport_offset() { return renderer_context.viewport_offset; }
  static unsigned get_viewport_width() { return renderer_context.viewport_size.x; }
  static unsigned get_viewport_height() { return renderer_context.viewport_size.y; }

private:

  // Config
  static RendererConfig renderer_config;
};
}
