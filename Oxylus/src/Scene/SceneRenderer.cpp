#include "SceneRenderer.h"

#include "Scene.h"

#include "Core/Application.h"
#include "Core/Entity.h"

#include "Render/DefaultRenderPipeline.h"
#include "Render/Vulkan/Renderer.h"

namespace Oxylus {
void SceneRenderer::init() {
  m_render_pipeline = create_ref<DefaultRenderPipeline>("DefaultRenderPipeline");
  Renderer::renderer_context.render_pipeline = m_render_pipeline;
  m_render_pipeline->init();
  m_render_pipeline->on_dispatcher_events(dispatcher);
}

void SceneRenderer::update() const {
  OX_SCOPED_ZONE;
  // Mesh
  {
    OX_SCOPED_ZONE_N("Mesh System");
    const auto view = m_scene->m_registry.view<TransformComponent, MeshRendererComponent, MaterialComponent, TagComponent>();
    for (const auto&& [entity, transform, meshrenderer, material, tag] : view.each()) {
      // TODO : temporary
      if (!meshrenderer.mesh_geometry->animations.empty()) {
        static auto animationTimer = 0.0f;
        animationTimer += Application::get_timestep() * 0.1f;
        if (animationTimer > meshrenderer.mesh_geometry->animations[0].end) {
          animationTimer -= meshrenderer.mesh_geometry->animations[0].end;
        }
        meshrenderer.mesh_geometry->update_animation(0, animationTimer);
      }

      auto e = Entity(entity, m_scene);
      auto parent = e.get_parent();
      bool parentEnabled = true;
      if (parent)
        parentEnabled = parent.get_component<TagComponent>().enabled;
      if (tag.enabled && parentEnabled && !material.materials.empty()) {
        auto render_object = MeshData{meshrenderer.mesh_geometry.get(), e.get_world_transform(), material.materials, meshrenderer.submesh_index};
        m_render_pipeline->on_register_render_object(render_object);
      }
    }
  }

  // Lighting
  {
    OX_SCOPED_ZONE_N("Lighting System");
    // Scene lights
    {
      const auto view = m_scene->m_registry.view<TransformComponent, LightComponent>();
      for (auto&& [e,tc, lc] : view.each()) {
        Entity entity = {e, m_scene};
        if (!entity.get_component<TagComponent>().enabled)
          continue;
        auto lighting_data = LightingData{
          Vec4{tc.translation, lc.intensity},
          Vec4{lc.color, lc.range},
          Vec4{tc.rotation, 1.0f}
        };
        m_render_pipeline->on_register_light(lighting_data, lc.type);
      }
    }
  }

  // TODO: (very outdated, currently not working)
  // Particle system 
  {
    OX_SCOPED_ZONE_N("Particle System");
    const auto particleSystemView = m_scene->m_registry.view<TransformComponent, ParticleSystemComponent>();
    for (auto&& [e, tc, psc] : particleSystemView.each()) {
      psc.system->on_update(Application::get_timestep(), tc.translation);
      psc.system->on_render();
    }
  }
}
}
