#pragma once

#include "Event/Event.h"
#include "Utils/Timestep.h"
#include "Core/Keycodes.h"

#include <string>

namespace JPH {
class ContactSettings;
class ContactManifold;
class Body;
}

namespace Oxylus {
class Scene;

class System {
public:
  std::string Name;

  System() = default;
  System(std::string name) : Name(std::move(name)) { }
  virtual ~System() = default;

  /// Scene systems: Called right after when the scene gets initalized.
  /// Engine systems: Called right before core is initalized.
  virtual void on_init() { }

  /// Called at the end of the core update loop.
  virtual void on_update() { }

  /// Called before physic system is updated.
  virtual void on_update(Scene* scene, Timestep delta_time) { }

  /// Called every fixed frame-rate frame with the frequency of the physics system
  virtual void on_fixed_update(Scene* scene, Timestep delta_time) { }

  /// Called after physics system is updated.
  virtual void post_on_update(Scene* scene, Timestep delta_time) { }

  /// Called in the main imgui loop which is right before `OnUpdate`
  virtual void on_imgui_render() { }

  /// Called in the main imgui loop which is right after `OnUpdate`
  virtual void on_imgui_render(Scene* scene, Timestep deltaTime) { }

  /// Called right after main loop is finished before the core shutdown process.
  virtual void on_shutdown() { }

  /// Physics interfaces
  virtual void on_contact_added(Scene* scene, const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, const JPH::ContactSettings& settings) { }
  virtual void on_contact_persisted(Scene* scene, const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, const JPH::ContactSettings& settings) { }

  /// Input interfaces
  virtual void on_key_pressed(KeyCode key) { }
  virtual void on_key_released(KeyCode key) { }

  void set_dispatcher(EventDispatcher* dispatcher) { m_Dispatcher = dispatcher; }

protected:
  EventDispatcher* m_Dispatcher = nullptr;
};
}
