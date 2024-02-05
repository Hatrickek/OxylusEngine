#include "SceneRenderer.h"

#include <future>
#include <execution>

#include "Scene.h"

#include "Core/Application.h"

#include "Physics/JoltHelpers.h"

#include "Scene/Entity.h"

#include "Render/DebugRenderer.h"
#include "Render/DefaultRenderPipeline.h"
#include "Render/Vulkan/Renderer.h"
#include "Render/Vulkan/VulkanContext.h"

namespace Ox {
void SceneRenderer::init() {
  OX_SCOPED_ZONE;
  if (!m_render_pipeline)
    m_render_pipeline = create_shared<DefaultRenderPipeline>("DefaultRenderPipeline");
  Renderer::renderer_context.render_pipeline = m_render_pipeline;
  m_render_pipeline->init(*VulkanContext::get()->superframe_allocator);
  m_render_pipeline->on_dispatcher_events(dispatcher);
}

void SceneRenderer::update() const {
  OX_SCOPED_ZONE;

  // Mesh System
  {
    OX_SCOPED_ZONE_N("Mesh System");
    const auto mesh_view = m_scene->m_registry.view<TransformComponent, MeshComponent, MaterialComponent, TagComponent>();
    for (const auto&& [entity, transform, mesh_component, material, tag] : mesh_view.each()) {
      if (!tag.enabled)
        continue;
      auto e = Entity(entity, m_scene);
      const auto world_transform = e.get_world_transform();
      mesh_component.transform = world_transform;
      mesh_component.aabb = mesh_component.mesh_base->linear_nodes[mesh_component.node_index]->aabb.get_transformed(world_transform);
      m_render_pipeline->on_register_render_object(mesh_component);

      if (RendererCVar::cvar_enable_debug_renderer.get() && RendererCVar::cvar_draw_bounding_boxes.get()) {
        DebugRenderer::draw_aabb(mesh_component.aabb, Vec4(1, 1, 1, 0.5f));
      }
    }
  }

  {
    OX_SCOPED_ZONE_N("Draw physics shapes");
    if (RendererCVar::cvar_enable_debug_renderer.get() && RendererCVar::cvar_draw_physics_shapes.get()) {
      const auto mesh_collider_view = m_scene->m_registry.view<TransformComponent, RigidbodyComponent, TagComponent>();
      for (const auto&& [entity, transform, rb, tag] : mesh_collider_view.each()) {
        if (!tag.enabled)
          continue;

        const auto* body = static_cast<const JPH::Body*>(rb.runtime_body);
        if (body) {
          auto aabb = convert_jolt_aabb(body->GetShape()->GetLocalBounds());
          DebugRenderer::draw_aabb(aabb, Vec4(0, 1, 0, 1.0f));
        }
      }
    }
  }

  // Animation system
  {
    OX_SCOPED_ZONE_N("Animated Mesh System");
    const auto animation_view = m_scene->m_registry.view<TransformComponent, MeshComponent, AnimationComponent, TagComponent>();
    for (const auto&& [entity, transform, mesh_renderer, animation, tag] : animation_view.each()) {
      if (!animation.animations.empty()) {
        animation.animation_timer += Application::get_timestep() * animation.animation_speed;
        if (animation.animation_timer > mesh_renderer.mesh_base->animations[animation.current_animation_index]->end) {
          animation.animation_timer -= mesh_renderer.mesh_base->animations[animation.current_animation_index]->end;
        }
        mesh_renderer.mesh_base->update_animation(animation.current_animation_index, animation.animation_timer);
      }
    }
  }

  // Lighting
  {
    OX_SCOPED_ZONE_N("Lighting System");
    const auto lighting_view = m_scene->m_registry.view<TransformComponent, LightComponent>();
    for (auto&& [e,tc, lc] : lighting_view.each()) {
      Entity entity = {e, m_scene};
      if (!entity.get_component<TagComponent>().enabled)
        continue;
      m_render_pipeline->on_register_light(tc, lc);
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
