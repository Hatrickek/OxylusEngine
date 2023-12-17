#pragma once
#include "Scene/Components.h"
#include "Event/Event.h"

namespace Oxylus {
class RenderPipeline;
class Scene;

class SceneRenderer {
public:
  EventDispatcher dispatcher;

  SceneRenderer(Scene* scene) : m_scene(scene) {}
  ~SceneRenderer() = default;

  void init();
  void update() const;

  Ref<RenderPipeline> get_render_pipeline() const { return m_render_pipeline; }
  void set_render_pipeline(const Ref<RenderPipeline>& render_pipeline) { m_render_pipeline = render_pipeline;}

private:
  Scene* m_scene;
  Ref<RenderPipeline> m_render_pipeline = nullptr;

  friend class Scene;
};
}
