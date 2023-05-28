#pragma once

#include "Event/Event.h"
#include "Utils/TimeStep.h"

#include <string>

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
    virtual void OnUpdate() { }
    virtual void OnUpdate(Scene* scene, Timestep deltaTime) { }
    virtual void OnImGuiRender() { }
    virtual void OnDebugRender() { }
    virtual void OnShutdown() { }
    void SetDispatcher(EventDispatcher* dispatcher) { m_Dispatcher = dispatcher; }

  protected:
    EventDispatcher* m_Dispatcher = nullptr;
  };
}
