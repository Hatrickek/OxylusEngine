#include "src/oxpch.h"
#include "SceneRenderer.h"

#include "Core/Components.h"
#include "Core/Entity.h"

#include "Render/Vulkan/VulkanRenderer.h"

#include "Utils/Profiler.h"
#include "Utils/TimeStep.h"

namespace Oxylus {
  void SceneRenderer::Init(Scene* scene) {
    m_Scene = scene;

    m_RenderPipeline = VulkanRenderer::GetDefaultRenderPipeline();
    m_RenderPipeline->OnDispatcherEvents(Dispatcher);
    VulkanRenderer::SetRenderGraph(m_RenderPipeline->GetRenderGraph());
  }

  void SceneRenderer::Render() const {
    ZoneScoped;

    m_RenderPipeline->OnRender(m_Scene);
  }
}
