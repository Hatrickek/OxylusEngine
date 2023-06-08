#pragma once

#include "Event/Event.h"
#include "Utils/TimeStep.h"

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
    virtual void OnInit() { }

    /// Called at the end of the core update loop.
    virtual void OnUpdate() { }

    /// Called before physic system is updated.
    virtual void OnUpdate(Scene* scene, Timestep deltaTime) { }

    /// Called after physics system is updated.
    virtual void PostOnUpdate(Scene* scene, Timestep deltaTime) { }

    /// Called in the main imgui loop which is right before `OnUpdate`
    virtual void OnImGuiRender() { }

    /// Called in the main imgui loop which is right after `OnUpdate`
    virtual void OnImGuiRender(Scene* scene, Timestep deltaTime) { }

    /// Called right after main loop is finished before the core shutdown process.
    virtual void OnShutdown() { }

    /// Physics interfaces
    virtual void OnContactAdded(Scene* scene, const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, const JPH::ContactSettings& settings) { }

    void SetDispatcher(EventDispatcher* dispatcher) { m_Dispatcher = dispatcher; }

  protected:
    EventDispatcher* m_Dispatcher = nullptr;
  };
}
