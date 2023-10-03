#include "SceneRenderer.h"

#include "Render/DefaultRenderPipeline.h"

namespace Oxylus {
void SceneRenderer::init(Scene* scene) {
  m_RenderPipeline = VulkanRenderer::renderer_context.default_render_pipeline;
  VulkanRenderer::renderer_context.render_pipeline = m_RenderPipeline;
  m_RenderPipeline->init(scene);
  m_RenderPipeline->on_dispatcher_events(dispatcher);
}
}
