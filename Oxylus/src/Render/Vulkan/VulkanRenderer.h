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
    Ref<RenderPipeline> m_RenderPipeline = nullptr;
    Ref<DefaultRenderPipeline> m_DefaultRenderPipeline = nullptr;
    Camera* m_CurrentCamera = nullptr;
  } s_RendererContext;

  static struct RendererData {
    struct Vertex {
      Vec3 Position{};
      Vec3 Normal{};
      Vec2 UV{};
      Vec4 Color{};
      Vec4 Joint0{};
      Vec4 Weight0{};
      Vec4 Tangent{};
    };
  } s_RendererData;

  struct LightingData {
    Vec4 PositionAndIntensity;
    Vec4 ColorAndRadius;
    Vec4 Rotation;
  };

  struct MeshData {
    Mesh* MeshGeometry;
    std::vector<Ref<Material>> Materials;
    Mat4 Transform;
    uint32_t SubmeshIndex = 0;

    MeshData(Mesh* mesh,
             const Mat4& transform,
             const std::vector<Ref<Material>>& materials,
             const uint32_t submeshIndex) : MeshGeometry(mesh), Materials(materials), Transform(transform),
                                            SubmeshIndex(submeshIndex) {}
  };

  static void Init();
  static void Shutdown();

  // Drawing
  static void Draw(VulkanContext* context, ImGuiLayer* imguiLayer, LayerStack& layerStack, Ref<SystemManager>& systemManager);
  static void RenderNode(const Mesh::Node* node, vuk::CommandBuffer& commandBuffer, const std::function<bool(Mesh::Primitive* prim)>& perMeshFunc);
  static void RenderMesh(const MeshData& mesh, vuk::CommandBuffer& commandBuffer, const std::function<bool(Mesh::Primitive* prim)>& perMeshFunc);

  // Utils
  static std::pair<vuk::Unique<vuk::Image>, vuk::Future> GenerateCubemapFromEquirectangular(const vuk::Texture& cubemap);
    
  // TODO(hatrickek): Temporary
  static void SetCamera(Camera& camera);

private:
  // Config
  static RendererConfig s_RendererConfig;
};
}
