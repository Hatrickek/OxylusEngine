#include "SceneRenderer.h"

#include <future>
#include <execution>

#include "Scene.h"

#include "Core/Application.h"
#include "Core/Entity.h"

#include "Render/DefaultRenderPipeline.h"
#include "Render/Vulkan/Renderer.h"
#include "Render/Vulkan/VulkanContext.h"

namespace Oxylus {
void SceneRenderer::init() {
  if (!m_render_pipeline)
    m_render_pipeline = create_ref<DefaultRenderPipeline>("DefaultRenderPipeline");
  Renderer::renderer_context.render_pipeline = m_render_pipeline;
  m_render_pipeline->init(*VulkanContext::get()->superframe_allocator);
  m_render_pipeline->on_dispatcher_events(dispatcher);
}

void SceneRenderer::update() const {
  OX_SCOPED_ZONE;

  // Mesh system
  const auto mesh_view = m_scene->m_registry.view<TransformComponent, MeshRendererComponent, MaterialComponent, TagComponent>();
  auto mesh_system_task = std::async(std::launch::async,
    [&] {
      OX_SCOPED_ZONE_N("Mesh System");
      for (const auto&& [entity, transform, meshrenderer, material, tag] : mesh_view.each()) {
        auto e = Entity(entity, m_scene);
        auto parent = e.get_parent();
        bool parentEnabled = true;
        if (parent)
          parentEnabled = parent.get_component<TagComponent>().enabled;
        if (tag.enabled && parentEnabled && !material.materials.empty()) {
          m_render_pipeline->on_register_render_object(MeshData{
            .index_count = (uint32_t)meshrenderer.mesh_geometry->indices.size(),
            .vertex_count = (uint32_t)meshrenderer.mesh_geometry->vertices.size(),
            .mesh_geometry = meshrenderer.mesh_geometry.get(),
            .materials = material.materials,
            .transform = e.get_world_transform(),
            .submesh_index = meshrenderer.submesh_index
          });
        }
      }
    });

  // Animated Mesh System
  const auto animation_view = m_scene->m_registry.view<TransformComponent, MeshRendererComponent, AnimationComponent, TagComponent>();
  auto animation_system_task = std::async(std::launch::async,
    [&] {
      OX_SCOPED_ZONE_N("Animated Mesh System");
      for (const auto&& [entity, transform, mesh_renderer, animation, tag] : animation_view.each()) {
        if (!animation.animations.empty()) {
          animation.animation_timer += Application::get_timestep() * animation.animation_speed;
          if (animation.animation_timer > mesh_renderer.mesh_geometry->animations[animation.current_animation_index]->end) {
            animation.animation_timer -= mesh_renderer.mesh_geometry->animations[animation.current_animation_index]->end;
          }
          mesh_renderer.mesh_geometry->update_animation(animation.current_animation_index, animation.animation_timer);
        }
      }
    });

  // Lighting
  const auto lighting_view = m_scene->m_registry.view<TransformComponent, LightComponent>();
  auto lighting_system_task = std::async(std::launch::async,
    [&] {
      OX_SCOPED_ZONE_N("Lighting System");
      for (auto&& [e,tc, lc] : lighting_view.each()) {
        Entity entity = {e, m_scene};
        if (!entity.get_component<TagComponent>().enabled)
          continue;
        auto lighting_data = LightingData{
          Vec4{tc.position, lc.intensity},
          Vec4{lc.color, lc.range},
          Vec4{tc.rotation, 1.0f}
        };
        m_render_pipeline->on_register_light(lighting_data, lc.type);
      }
    });

  // TODO: (very outdated, currently not working)
  // Particle system 
  const auto particle_system_view = m_scene->m_registry.view<TransformComponent, ParticleSystemComponent>();
  auto particle_system_task = std::async(std::launch::async,
    [&] {
      for (auto&& [e, tc, psc] : particle_system_view.each()) {
        psc.system->on_update(Application::get_timestep(), tc.position);
        psc.system->on_render();
      }
    });

  mesh_system_task.get();
  animation_system_task.get();
  lighting_system_task.get();
  particle_system_task.get();
}
}
