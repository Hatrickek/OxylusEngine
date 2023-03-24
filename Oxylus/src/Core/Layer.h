#pragma once

#include <string>
#include <Event/Event.h>

namespace Oxylus {
  class Layer {
  public:
    Layer(const std::string& name = "Layer");
    virtual ~Layer() = default;
    
    virtual void OnAttach(EventDispatcher& dispatcher) { }
    virtual void OnDetach() { }
    virtual void OnUpdate(float deltaTime) { }
    virtual void OnImGuiRender() { }

    const std::string& GetName() const { return m_DebugName; }

  protected:
    std::string m_DebugName;
  };
}
