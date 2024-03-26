﻿#pragma once
#include <mutex>

#include <vuk/Future.hpp>
#include "Event/Event.hpp"

#include "Scene/Components.hpp"
#include "Core/Base.hpp"

namespace vuk {
struct SampledImage;
}

namespace ox {
class VkContext;
class Scene;

class RenderPipeline {
public:
  RenderPipeline(std::string name) : m_name(std::move(name)), attach_swapchain(true) {}

  virtual ~RenderPipeline() = default;

  virtual void init(vuk::Allocator& allocator) = 0;
  virtual void shutdown() = 0;

  virtual Unique<vuk::Future> on_render(vuk::Allocator& frame_allocator, const vuk::Future& target, vuk::Dimension3D dim) = 0;

  virtual void on_dispatcher_events(EventDispatcher& dispatcher) {}

  virtual void on_update(Scene* scene) {}
  virtual void register_mesh_component(const MeshComponent& render_object) {}
  virtual void register_light(const LightComponent& light) {}
  virtual void register_camera(Camera* camera) {}

  virtual void enqueue_future(vuk::Future&& fut);
  virtual void wait_for_futures(vuk::Allocator& allocator);

  virtual void detach_swapchain(vuk::Dimension3D dim, Vec2 offset = {});
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
  virtual Vec2 get_viewport_offset() { return viewport_offset; }

  virtual void submit_rg_future(vuk::Future&& future) { rg_futures.emplace_back(future); }
  virtual std::vector<vuk::Future>& get_rg_futures() { return rg_futures; }
  virtual void clear_rg_futures() { rg_futures.clear(); }

  virtual void set_compiler(vuk::Compiler* compiler) { this->compiler = compiler; }
  virtual vuk::Compiler* get_compiler() { return compiler; }

protected:
  std::string m_name = {};
  bool attach_swapchain = false;
  vuk::Dimension3D dimension = {};
  Vec2 viewport_offset = {};
  Shared<vuk::SampledImage> final_image = nullptr;
  Shared<vuk::RenderGraph> frame_render_graph = nullptr;
  vuk::Name final_attachment_name;
  vuk::Allocator* m_frame_allocator;
  vuk::Compiler* compiler = nullptr;
  std::mutex setup_lock;
  std::vector<vuk::Future> futures;
  std::vector<vuk::Future> rg_futures;
};
}
