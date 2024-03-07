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

namespace ox {
class Scene;

class System {
public:
  std::string name;

  System() = default;
  System(std::string name) : name(std::move(name)) { }
  virtual ~System() = default;

  /// Scene systems: Called right after when the scene gets initalized.
  virtual void on_init() { }

  /// Called when the system is destroyed.
  virtual void on_release(Scene* scene) { }

  /// Called at the end of the core update loop.
  virtual void on_update() { }

  /// Called after physic system is updated.
  virtual void on_update(Scene* scene, const Timestep& delta_time) { }

  /// Called every fixed frame-rate frame with the frequency of the physics system
  virtual void on_fixed_update(Scene* scene, float delta_time) { }

  /// Called before physics system is updated.
  virtual void pre_on_update(Scene* scene, const Timestep& delta_time) { }

  /// Called in the main imgui loop which is right after `App::on_update`
  virtual void on_imgui_render() { }

  /// Called in the main imgui loop which is right after `App::on_update`
  virtual void on_imgui_render(Scene* scene, const Timestep& delta_time) { }

  /// Called right after main loop is finished before the core shutdown process.
  virtual void on_shutdown() { }

  /// Physics interfaces
  virtual void on_contact_added(Scene* scene, const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, const JPH::ContactSettings& settings) { }
  virtual void on_contact_persisted(Scene* scene, const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, const JPH::ContactSettings& settings) { }

  void set_dispatcher(EventDispatcher* dispatcher) { m_dispatcher = dispatcher; }

protected:
  EventDispatcher* m_dispatcher = nullptr;
};
}
