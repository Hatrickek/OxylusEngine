#pragma once

#include <string>
#include <Event/Event.h>

#include "Utils/Timestep.h"
#include "Keycodes.h"

namespace Oxylus {
class Layer {
public:
  Layer(const std::string& name = "Layer");
  virtual ~Layer() = default;

  virtual void OnAttach(EventDispatcher& dispatcher) { }
  virtual void OnDetach() { }
  virtual void OnUpdate(Timestep deltaTime) { }
  virtual void OnImGuiRender() { }

  virtual void OnKeyPressed(KeyCode key) {}
  virtual void OnKeyReleased(KeyCode key) {}

  const std::string& GetName() const { return m_DebugName; }

protected:
  std::string m_DebugName;
};
}
