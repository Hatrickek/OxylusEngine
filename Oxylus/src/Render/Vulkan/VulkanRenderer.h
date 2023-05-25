#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>

#include "DescriptorPoolManager.h"
#include "CommandPoolManager.h"
#include "VulkanCommandBuffer.h"
#include "VulkanSwapchain.h"
#include "VulkanPipeline.h"
#include "Core/Components.h"

#include "Render/Camera.h"
#include "Render/DefaultRenderPipeline.h"
#include "Render/RendererConfig.h"

namespace Oxylus {
  using vDT = vk::DescriptorType;
  using vSS = vk::ShaderStageFlagBits;
  using vPS = vk::PipelineStageFlagBits;
  using vDF = vk::DependencyFlagBits;
  using vMP = vk::MemoryPropertyFlagBits;
  using vBU = vk::BufferUsageFlagBits;

  class VulkanRenderer {
  public:
    static struct RendererContext {
      Ref<RenderGraph> RenderGraph = nullptr;
      VulkanCommandBuffer TimelineCommandBuffer;

      //Camera
      Camera* CurrentCamera = nullptr;
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

      vk::DescriptorSetLayout ImageDescriptorSetLayout;
    } s_RendererData;

    static struct Pipelines {
      VulkanPipeline QuadPipeline;
      VulkanPipeline UIPipeline;
    } s_Pipelines;

    static VulkanSwapchain SwapChain;

    static void Init();
    static void Shutdown();

    static Ref<DefaultRenderPipeline> GetDefaultRenderPipeline() { return s_DefaultPipeline; }

    // TODO(hatrickek): Temporary solution
    static void SetRenderGraph(const Ref<RenderGraph>& renderGraph) { s_RendererContext.RenderGraph = renderGraph; }

    static void ResizeBuffers();

    // Queue
    static void SubmitOnce(vk::CommandPool commandPool, const std::function<void(VulkanCommandBuffer& cmdBuffer)>& submitFunc);

    // Drawing
    static void Draw();
    static void DrawFullscreenQuad(const vk::CommandBuffer& commandBuffer, bool bindVertex = false);

    // TODO(hatrickek): Temporary
    static void SetCamera(Camera& camera);

    static void OnResize();
    static void WaitDeviceIdle();
    static void WaitGraphicsQueueIdle();

  private:
    // Default render pipeline
    static Ref<DefaultRenderPipeline> s_DefaultPipeline;

    // Debug renderer
    static Ref<DebugRenderer> s_DebugRenderer;

    // Config
    static RendererConfig s_RendererConfig;

    // Pool Managers
    static Ref<DescriptorPoolManager> s_DescriptorPoolManager;
    static Ref<CommandPoolManager> s_CommandPoolManager;
  };
}
