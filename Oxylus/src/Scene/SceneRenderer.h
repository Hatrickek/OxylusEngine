#pragma once
#include "Components.hpp"
#include "Event/Event.hpp"

namespace ox {
class RenderPipeline;
class Scene;

class SceneRenderer {
public:
  EventDispatcher dispatcher;

  SceneRenderer(Scene* scene) : m_scene(scene) {}
  ~SceneRenderer() = default;

  void init();
  void update() const;

  Shared<RenderPipeline> get_render_pipeline() const { return m_render_pipeline; }
  void set_render_pipeline(const Shared<RenderPipeline>& render_pipeline) { m_render_pipeline = render_pipeline;}

private:
  Scene* m_scene;
  Shared<RenderPipeline> m_render_pipeline = nullptr;

  friend class Scene;
};
}
