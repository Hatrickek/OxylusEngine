#include "SceneRenderer.h"

#include "Core/Components.h"

#include "Render/Vulkan/VulkanRenderer.h"

#include "Utils/Profiler.h"

namespace Oxylus {
  void SceneRenderer::Init(Scene* scene) {
    m_Scene = scene;

    m_RenderPipeline = VulkanRenderer::GetDefaultRenderPipeline();
    m_RenderPipeline->OnDispatcherEvents(Dispatcher);
    VulkanRenderer::SetRenderGraph(m_RenderPipeline->GetRenderGraph());
  }

  void SceneRenderer::Render() const {
    OX_SCOPED_ZONE;

    m_RenderPipeline->OnRender(m_Scene);
  }
}
