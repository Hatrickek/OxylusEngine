#include "SceneRenderer.h"

#include <future>
#include <execution>

#include "Scene.h"

#include "Core/Application.h"
#include "Core/Entity.h"

#include "Render/DebugRenderer.h"
#include "Render/DefaultRenderPipeline.h"
#include "Render/Vulkan/Renderer.h"
#include "Render/Vulkan/VulkanContext.h"

namespace Oxylus {
void SceneRenderer::init() {
  OX_SCOPED_ZONE;
  if (!m_render_pipeline)
    m_render_pipeline = create_ref<DefaultRenderPipeline>("DefaultRenderPipeline");
  Renderer::renderer_context.render_pipeline = m_render_pipeline;
  m_render_pipeline->init(*VulkanContext::get()->superframe_allocator);
  m_render_pipeline->on_dispatcher_events(dispatcher);
}

void SceneRenderer::update() const {
  OX_SCOPED_ZONE;

  {
    const auto mesh_view = m_scene->m_registry.view<TransformComponent, MeshComponent, MaterialComponent, TagComponent>();
    OX_SCOPED_ZONE_N("Mesh System");
    for (const auto&& [entity, transform, mesh_component, material, tag] : mesh_view.each()) {
      if (tag.enabled) {
        auto e = Entity(entity, m_scene);
        mesh_component.transform = e.get_world_transform();
        m_render_pipeline->on_register_render_object(mesh_component);

        //if (RendererCVar::cvar_draw_bounding_boxes.get()) {
        //  for (const auto* node : mesh_component.original_mesh->linear_nodes)
        //    if (node->mesh_data)
        //      DebugRenderer::draw_bb(node->aabb);
        //}
      }
    }
  }

  {
    const auto animation_view = m_scene->m_registry.view<TransformComponent, MeshComponent, AnimationComponent, TagComponent>();
    OX_SCOPED_ZONE_N("Animated Mesh System");
    for (const auto&& [entity, transform, mesh_renderer, animation, tag] : animation_view.each()) {
      if (!animation.animations.empty()) {
        animation.animation_timer += Application::get_timestep() * animation.animation_speed;
        if (animation.animation_timer > mesh_renderer.original_mesh->animations[animation.current_animation_index]->end) {
          animation.animation_timer -= mesh_renderer.original_mesh->animations[animation.current_animation_index]->end;
        }
        mesh_renderer.original_mesh->update_animation(animation.current_animation_index, animation.animation_timer);
      }
    }
  }

  // Lighting
  {
    const auto lighting_view = m_scene->m_registry.view<TransformComponent, LightComponent>();
    OX_SCOPED_ZONE_N("Lighting System");
    for (auto&& [e,tc, lc] : lighting_view.each()) {
      Entity entity = {e, m_scene};
      if (!entity.get_component<TagComponent>().enabled)
        continue;
      auto lighting_data = LightingData{
        .position_intensity = Vec4{tc.position, lc.intensity},
        .color_radius = Vec4{lc.color, lc.range},
        .rotation_type = Vec4{tc.rotation, lc.type}
      };
      m_render_pipeline->on_register_light(lighting_data, lc.type);
    }
  }

  // TODO: (very outdated, currently not working)
  // Particle system 
  const auto particle_system_view = m_scene->m_registry.view<TransformComponent, ParticleSystemComponent>();
  for (auto&& [e, tc, psc] : particle_system_view.each()) {
    psc.system->on_update(Application::get_timestep(), tc.position);
    psc.system->on_render();
  }
}
}
