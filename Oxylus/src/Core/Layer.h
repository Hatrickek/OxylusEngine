#pragma once

#include <string>
#include <Event/Event.h>

#include "Utils/Timestep.h"
#include "Keycodes.h"

namespace ox {
class Layer {
public:
  Layer(const std::string& name = "Layer");
  virtual ~Layer() = default;

  virtual void on_attach(EventDispatcher& dispatcher) { }
  virtual void on_detach() { }
  virtual void on_update(const Timestep& delta_time) { }
  virtual void on_imgui_render() { }

  virtual void on_key_pressed(KeyCode key) {}
  virtual void on_key_released(KeyCode key) {}

  const std::string& get_name() const { return debug_name; }

protected:
  std::string debug_name;
};
}
