#pragma once
#include "Core/Components.h"
#include "Event/Event.h"

namespace Oxylus {
class RenderPipeline;
class Scene;

class SceneRenderer {
public:
  struct SkyboxLoadEvent {
    Ref<TextureAsset> CubeMap = nullptr;
  };

  struct ProbeChangeEvent {
    PostProcessProbe Probe;
  };

  EventDispatcher Dispatcher;

  SceneRenderer() = default;
  ~SceneRenderer() = default;

  void Init(Scene* scene);

  Ref<RenderPipeline> GetRenderPipeline() const { return m_RenderPipeline; }

private:
  Ref<RenderPipeline> m_RenderPipeline = nullptr;
};
}
