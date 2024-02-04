#pragma once
#include <mutex>

#include "Event/Event.h"
#include <vuk/Future.hpp>

#include "RendererData.h"

#include "Core/Base.h"
#include "Scene/Components.h"

namespace vuk {
struct SampledImage;
}

namespace Ox {
class VulkanContext;
class Scene;

class RenderPipeline {
public:
  RenderPipeline(std::string name) : m_name(std::move(name)), attach_swapchain(true) { }

  virtual ~RenderPipeline() = default;

  virtual void init(vuk::Allocator& allocator) = 0;
  virtual void shutdown() = 0;

  virtual Unique<vuk::Future> on_render(vuk::Allocator& frame_allocator, const vuk::Future& target, vuk::Dimension3D dim) = 0;

  virtual void on_dispatcher_events(EventDispatcher& dispatcher) { }

  virtual void on_update(Scene* scene) { }
  virtual void on_register_render_object(const MeshComponent& render_object) { }
  virtual void on_register_light(const TransformComponent& transform, const LightComponent& light) { }
  virtual void on_register_camera(Camera* camera) { }

  virtual void enqueue_future(vuk::Future&& fut);
  virtual void wait_for_futures(vuk::Allocator& allocator);

  virtual void detach_swapchain(vuk::Dimension3D dim);
  virtual bool is_swapchain_attached() { return attach_swapchain; }

  virtual Shared<vuk::SampledImage> get_final_image() { return final_image; }
  virtual void set_final_image(Shared<vuk::SampledImage>& image) { final_image = image; }

  virtual Shared<vuk::RenderGraph> get_frame_render_graph() { return frame_render_graph; }
  virtual void set_frame_render_graph(Shared<vuk::RenderGraph> render_graph) { frame_render_graph = render_graph; }

  virtual vuk::Allocator* get_frame_allocator() { return m_frame_allocator; }
  virtual void set_frame_allocator(vuk::Allocator* allocator) { m_frame_allocator = allocator; }

  virtual vuk::Name get_final_attachment_name() { return final_attachment_name; }
  virtual void set_final_attachment_name(const vuk::Name name) { final_attachment_name = name; }

  virtual const std::string& get_name() { return m_name; }
  virtual vuk::Dimension3D get_dimension() { return dimension; }

  virtual void submit_rg_future(vuk::Future&& future) { rg_futures.emplace_back(future); }
  virtual std::vector<vuk::Future>& get_rg_futures() { return rg_futures; }
  virtual void clear_rg_futures() { rg_futures.clear(); }

protected:
  std::string m_name = {};
  bool attach_swapchain = false;
  vuk::Dimension3D dimension = {};
  Shared<vuk::SampledImage> final_image = nullptr;
  Shared<vuk::RenderGraph> frame_render_graph = nullptr;
  vuk::Name final_attachment_name;
  vuk::Allocator* m_frame_allocator;
  std::mutex setup_lock;
  std::vector<vuk::Future> futures;
  std::vector<vuk::Future> rg_futures;
};
}
