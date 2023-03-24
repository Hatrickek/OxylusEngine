#include "src/oxpch.h"
#include "SceneRenderer.h"

#include "Core/Components.h"
#include "Core/Entity.h"

#include "Render/Vulkan/VulkanRenderer.h"

#include "Utils/Profiler.h"
#include "Utils/Timestep.h"

namespace Oxylus {
  void SceneRenderer::Render(Scene& scene) const {
    ZoneScoped;

    //Mesh
    {
      ZoneScopedN("Mesh System");
      const auto view = scene.m_Registry.view<TransformComponent, MeshRendererComponent, MaterialComponent, TagComponent>();
      for (const auto&& [entity, transform, meshrenderer, material, tag] : view.each()) {
        if (tag.Enabled)
          VulkanRenderer::SubmitMesh(*meshrenderer.MeshGeometry, Entity(entity, &scene).GetWorldTransform(),
                                     material.Materials, meshrenderer.SubmesIndex);
      }
    }

    //Particle system
    {
      ZoneScopedN("Particle System");
      const auto particleSystemView = scene.m_Registry.view<TransformComponent, ParticleSystemComponent>();
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
        const auto view = scene.m_Registry.view<LightComponent>();
        lights.reserve(view.size());
        for (auto&& [e, lc] : view.each()) {
          Entity entity = {e, &scene};
          if (!entity.GetComponent<TagComponent>().Enabled)
            continue;
          lights.emplace_back(entity);
        }
        VulkanRenderer::SubmitLights(std::move(lights));
      }
      //Sky light
      {
        const auto view = scene.m_Registry.view<SkyLightComponent>();
        if (!view.empty()) {
          const auto skylight = Entity(*view.begin(), &scene);
          VulkanRenderer::SubmitSkyLight(skylight);
        }
      }
    }
  }
}
