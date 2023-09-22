#include "SceneRenderer.h"

#include "Render/DefaultRenderPipeline.h"

namespace Oxylus {
void SceneRenderer::Init(Scene* scene) {
  m_RenderPipeline = VulkanRenderer::s_RendererContext.m_DefaultRenderPipeline;
  VulkanRenderer::s_RendererContext.m_RenderPipeline = m_RenderPipeline;
  m_RenderPipeline->Init(scene);
  m_RenderPipeline->OnDispatcherEvents(Dispatcher);
}
}
