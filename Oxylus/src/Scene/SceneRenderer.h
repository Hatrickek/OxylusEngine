#pragma once
#include "Core/Components.h"
#include "Event/Event.h"

namespace Oxylus {
class RenderPipeline;
class Scene;

class SceneRenderer {
public:
  SceneRenderer(Scene* scene) : m_scene(scene) {}
  ~SceneRenderer() = default;

  EventDispatcher dispatcher;
  void init();
  void update() const;

  Ref<RenderPipeline> get_render_pipeline() const { return m_render_pipeline; }

private:
  Scene* m_scene;
  Ref<RenderPipeline> m_render_pipeline = nullptr;

  friend class Scene;
};
}
