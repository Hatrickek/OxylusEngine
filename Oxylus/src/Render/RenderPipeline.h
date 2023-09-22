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
  RenderPipeline(std::string name) : m_Name(std::move(name)), m_AttachSwapchain(true) {}
  virtual ~RenderPipeline() = default;

  virtual void Init(Scene* scene) {}
  virtual void Update(Scene* scene) {}
  virtual void Shutdown() {}

  virtual Scope<vuk::Future> OnRender(vuk::Allocator& frameAllocator, const vuk::Future& target) = 0;
  virtual void OnDispatcherEvents(EventDispatcher& dispatcher) {}

  virtual void EnqueueFuture(vuk::Future&& fut);
  virtual void WaitForFutures(VulkanContext* vkContext);

  virtual void DetachSwapchain(vuk::Dimension3D dimension);

  virtual bool IsSwapchainAttached() { return m_AttachSwapchain; }

  virtual Ref<vuk::SampledImage> GetFinalImage() { return m_FinalImage; }
  virtual void SetFinalImage(Ref<vuk::SampledImage>& image) { m_FinalImage = image; }

  virtual const std::string& GetName() { return m_Name; }
  virtual vuk::Dimension3D GetDimension() { return m_Dimension; }

protected:
  std::string m_Name = {};
  bool m_AttachSwapchain = false;
  vuk::Dimension3D m_Dimension = {};
  Ref<vuk::SampledImage> m_FinalImage = nullptr;
  std::mutex m_SetupLock;
  std::vector<vuk::Future> m_Futures;
};
}
