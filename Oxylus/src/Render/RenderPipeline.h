#pragma once
#include <mutex>

#include "Event/Event.h"
#include <vuk/Future.hpp>
#include "Core/Base.h"

namespace vuk {
struct SampledImage;
}

namespace Oxylus {
class VulkanContext;
class Scene;

class RenderPipeline {
public:
  RenderPipeline(std::string name) : m_name(std::move(name)), attach_swapchain(true) { }

  virtual ~RenderPipeline() = default;

  virtual void init(Scene* scene) { }
  virtual void update(Scene* scene) { }
  virtual void shutdown() { }

  virtual Scope<vuk::Future> on_render(vuk::Allocator& frameAllocator, const vuk::Future& target, vuk::Dimension3D dim) = 0;

  virtual void on_dispatcher_events(EventDispatcher& dispatcher) { }

  virtual void enqueue_future(vuk::Future&& fut);
  virtual void wait_for_futures(VulkanContext* vkContext);

  virtual void detach_swapchain(vuk::Dimension3D dim);
  virtual bool is_swapchain_attached() { return attach_swapchain; }

  virtual Ref<vuk::SampledImage> get_final_image() { return final_image; }
  virtual void set_final_image(Ref<vuk::SampledImage>& image) { final_image = image; }

  virtual const std::string& get_name() { return m_name; }
  virtual vuk::Dimension3D get_dimension() { return dimension; }

protected:
  std::string m_name = {};
  bool attach_swapchain = false;
  vuk::Dimension3D dimension = {};
  Ref<vuk::SampledImage> final_image = nullptr;
  std::mutex setup_lock;
  std::vector<vuk::Future> futures;
};
}
