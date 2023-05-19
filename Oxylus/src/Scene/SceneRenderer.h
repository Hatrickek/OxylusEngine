#pragma once
#include "Event/Event.h"
#include "Render/RenderPipeline.h"
#include "Render/Vulkan/VulkanImage.h"

namespace Oxylus {
  class Scene;

  class SceneRenderer {
  public:
    struct SkyboxLoadEvent {
      Ref<VulkanImage> CubeMap = nullptr;
    };

    struct ProbeChangeEvent {};

    EventDispatcher Dispatcher;

    SceneRenderer() = default;
    ~SceneRenderer() = default;

    void Init(Scene* scene);
    void Render() const;

    Ref<RenderPipeline> GetRenderPipeline() const { return m_RenderPipeline; }

  private:
    Ref<RenderPipeline> m_RenderPipeline = nullptr;
    Scene* m_Scene = nullptr;
  };
}
