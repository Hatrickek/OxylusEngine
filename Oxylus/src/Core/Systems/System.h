#pragma once

#include <Event/Event.h>

#include <string>

namespace Oxylus {
  class Scene;

  class System {
  public:
    std::string Name;

    System() = default;
    System(std::string name) : Name(std::move(name)) { }
    virtual ~System() = default;
    
    virtual void OnInit() { };
    virtual void OnUpdate() { }
    virtual void OnUpdate(Scene* scene) { }
    virtual void OnImGuiRender() { }
    virtual void OnShutdown() { };
    void SetDispatcher(EventDispatcher* dispatcher) { m_Dispatcher = dispatcher; }

  protected:
    EventDispatcher* m_Dispatcher = nullptr;
  };
}
