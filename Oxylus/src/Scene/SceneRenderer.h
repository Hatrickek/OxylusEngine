#pragma once
#include "Core/Components.h"
#include "Event/Event.h"

namespace Oxylus {
class RenderPipeline;
class Scene;

class SceneRenderer {
public:
  SceneRenderer() = default;
  ~SceneRenderer() = default;

  EventDispatcher dispatcher;
  void init(Scene* scene);

  Ref<RenderPipeline> get_render_pipeline() const { return m_RenderPipeline; }

private:
  Ref<RenderPipeline> m_RenderPipeline = nullptr;
};
}
