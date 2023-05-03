#include "src/oxpch.h"
#include "SceneRenderer.h"

#include "Core/Components.h"
#include "Core/Entity.h"

#include "Render/Vulkan/VulkanRenderer.h"

#include "Utils/Profiler.h"
#include "Utils/TimeStep.h"

namespace Oxylus {
  void SceneRenderer::Init(Scene& scene) {
    m_Scene = &scene;
    Dispatcher.sink<ProbeChangeEvent>().connect<&SceneRenderer::UpdateProbes>(*this);
    VulkanRenderer::s_RendererData.PostProcessBuffer.Sink<ProbeChangeEvent>(Dispatcher);
  }

  void SceneRenderer::Render() const {
    ZoneScoped;

    //Mesh
    {
      ZoneScopedN("Mesh System");
      const auto view = m_Scene->m_Registry.view<TransformComponent, MeshRendererComponent, MaterialComponent, TagComponent>();
      for (const auto&& [entity, transform, meshrenderer, material, tag] : view.each()) {
        if (tag.Enabled)
          VulkanRenderer::SubmitMesh(*meshrenderer.MeshGeometry, Entity(entity, m_Scene).GetWorldTransform(), material.Materials, meshrenderer.SubmesIndex);
      }
    }

    //Particle system
    {
      ZoneScopedN("Particle System");
      const auto particleSystemView = m_Scene->m_Registry.view<TransformComponent, ParticleSystemComponent>();
      for (auto&& [e, tc, psc] : particleSystemView.each()) {
        psc.System->OnUpdate(Timestep::GetDeltaTime(), tc.Translation);
        psc.System->OnRender();
      }
    }

    //Lighting
    {
      ZoneScopedN("Lighting System");
      //Scene lights
      {
        std::vector<Entity> lights;
        const auto view = m_Scene->m_Registry.view<LightComponent>();
        lights.reserve(view.size());
        for (auto&& [e, lc] : view.each()) {
          Entity entity = {e, m_Scene};
          if (!entity.GetComponent<TagComponent>().Enabled)
            continue;
          lights.emplace_back(entity);
        }
        VulkanRenderer::SubmitLights(std::move(lights));
      }
      //Sky light
      {
        const auto view = m_Scene->m_Registry.view<SkyLightComponent>();
        if (!view.empty()) {
          const auto skylight = Entity(*view.begin(), m_Scene);
          VulkanRenderer::SubmitSkyLight(skylight);
        }
      }
    }
  }

  void SceneRenderer::UpdateProbes() const {
    //Post Process
    {
      ZoneScopedN("PostProcess Probe System");
      const auto view = m_Scene->m_Registry.view < PostProcessProbe > ();
      if (!view.empty()) {
        //TODO: Check if the camera is inside this probe.
        for (const auto&& [entity, component] : view.each()) {
          auto& ubo = VulkanRenderer::s_RendererData.UBO_PostProcessParams;
          ubo.FilmGrain = {component.FilmGrainEnabled, component.FilmGrainIntensity};
          ubo.ChromaticAberration = {component.ChromaticAberrationEnabled, component.ChromaticAberrationIntensity};
          ubo.FilmGrain = {component.FilmGrainEnabled, component.FilmGrainIntensity};
          ubo.VignetteOffset.w = component.VignetteEnabled;
          ubo.VignetteColor.a = component.VignetteIntensity;
        }
      }
    }
  }
}
