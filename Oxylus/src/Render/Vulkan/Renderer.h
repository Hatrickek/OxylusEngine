#pragma once

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>

#include "Scene/Components.h"

#include "Render/RendererConfig.h"
#include "Render/RenderPipeline.h"

namespace Oxylus {
class DefaultRenderPipeline;
class SystemManager;
class LayerStack;
class ImGuiLayer;
class VulkanContext;

class Renderer {
public:
  static struct RendererContext {
    // TODO:
    // need to get rid of this and instead use the render pipeline from scenes directly.
    // currently this gets filled at scene creation 
    Shared<RenderPipeline> render_pipeline = nullptr;
    UVec2 viewport_size = {1600, 900};
  } renderer_context;

  struct RendererStats {
    uint32_t drawcall_count = 0;
    uint32_t drawcall_culled_count = 0;
    uint32_t indices_count = 0;
    uint32_t vertices_count = 0;
    uint32_t primitives_count = 0;
  };

  static void init();
  static void shutdown();

  // Drawing
  static void draw(VulkanContext* context, ImGuiLayer* imgui_layer, LayerStack& layer_stack, const Shared<SystemManager>& system_manager);

  static vuk::CommandBuffer& draw_indexed(vuk::CommandBuffer& command_buffer,
                                          size_t index_count,
                                          size_t instance_count,
                                          size_t first_index,
                                          int32_t vertex_offset,
                                          size_t first_instance);

  static UVec2 get_viewport_size() { return renderer_context.viewport_size; }
  static unsigned get_viewport_width() { return renderer_context.viewport_size.x; }
  static unsigned get_viewport_height() { return renderer_context.viewport_size.y; }

  static RendererStats& get_stats() { return renderer_stats; }
  static void reset_stats();

private:
  static RendererStats renderer_stats;

  // Config
  static RendererConfig renderer_config;
};
}
