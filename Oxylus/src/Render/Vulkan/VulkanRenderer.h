#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>

#include "Core/Components.h"

#include "Render/Camera.h"
#include "Render/RendererConfig.h"
#include "Render/RenderPipeline.h"

#include "Render/Mesh.h"

namespace Oxylus {
class DefaultRenderPipeline;
class SystemManager;
class LayerStack;
class ImGuiLayer;
class VulkanContext;

class VulkanRenderer {
public:
  static struct RendererContext {
    Ref<RenderPipeline> render_pipeline = nullptr;
    Ref<DefaultRenderPipeline> default_render_pipeline = nullptr;
    Camera* current_camera = nullptr;
    UVec2 viewport_size = {1600, 900};
  } renderer_context;

  struct LightingData {
    Vec4 position_and_intensity;
    Vec4 color_and_radius;
    Vec4 rotation;
  };

  struct MeshData {
    Mesh* mesh_geometry;
    std::vector<Ref<Material>> materials;
    Mat4 transform;
    uint32_t submesh_index = 0; // todo: get rid of this

    MeshData(Mesh* mesh,
             const Mat4& transform,
             const std::vector<Ref<Material>>& materials,
             const uint32_t submeshIndex) : mesh_geometry(mesh), materials(materials), transform(transform),
                                            submesh_index(submeshIndex) { }
  };

  static void init();
  static void shutdown();

  // Drawing
  static void draw(VulkanContext* context, ImGuiLayer* imgui_layer, LayerStack& layer_stack, const Ref<SystemManager>& system_manager);
  static void render_node(const Mesh::Node* node, vuk::CommandBuffer& command_buffer, const std::function<bool(Mesh::Primitive* prim, Mesh::MeshData* mesh_data)>& per_mesh_func);
  static void render_mesh(const MeshData& mesh, vuk::CommandBuffer& command_buffer, const std::function<bool(Mesh::Primitive* prim, Mesh::MeshData* mesh_data)>& per_mesh_func);

  static UVec2 get_viewport_size() { return renderer_context.viewport_size; }
  static unsigned get_viewport_width() { return renderer_context.viewport_size.x; }
  static unsigned get_viewport_height() { return renderer_context.viewport_size.y; }

  // TODO(hatrickek): Temporary
  static void set_camera(Camera& camera);

private:
  // Config
  static RendererConfig renderer_config;
};
}
