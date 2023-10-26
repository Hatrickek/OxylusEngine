#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>

#include "Core/Components.h"

#include "Render/Camera.h"
#include "Render/RendererConfig.h"
#include "Render/RenderPipeline.h"

#include "Render/Mesh.h"
#include "Render/RendererData.h"

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
    Ref<RenderPipeline> render_pipeline = nullptr; 
    UVec2 viewport_size = {1600, 900};
  } renderer_context;

  static void init();
  static void shutdown();

  // Drawing
  static void draw(VulkanContext* context, ImGuiLayer* imgui_layer, LayerStack& layer_stack, const Ref<SystemManager>& system_manager);
  static void render_node(const Mesh::Node* node, vuk::CommandBuffer& command_buffer, const std::function<bool(Mesh::Primitive* prim, Mesh::MeshData* mesh_data)>& per_mesh_func);
  static void render_mesh(const MeshData& mesh, vuk::CommandBuffer& command_buffer, const std::function<bool(Mesh::Primitive* prim, Mesh::MeshData* mesh_data)>& per_mesh_func);

  static UVec2 get_viewport_size() { return renderer_context.viewport_size; }
  static unsigned get_viewport_width() { return renderer_context.viewport_size.x; }
  static unsigned get_viewport_height() { return renderer_context.viewport_size.y; }

private:
  // Config
  static RendererConfig renderer_config;
};
}
