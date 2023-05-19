#pragma once
#include "Event/Event.h"
#include "Core/Base.h"

namespace Oxylus {
  class Scene;
  class VulkanImage;
  class RenderGraph;

  class RenderPipeline {
  public:
    virtual ~RenderPipeline() = default;

    virtual void OnInit() {}
    virtual void OnRender(Scene* scene) {}
    virtual void OnShutdown() {}

    virtual void OnDispatcherEvents(EventDispatcher& dispatcher) {};

    const virtual VulkanImage& GetFinalImage() = 0;

    Ref<RenderGraph> GetRenderGraph() const { return m_RenderGraph; }

  protected:
    virtual void InitRenderGraph() = 0;

  protected:
    Ref<RenderGraph> m_RenderGraph = nullptr;
  };
}
