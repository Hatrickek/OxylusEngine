#pragma once

#include "Entity.h"

namespace Oxylus {
  class ScriptableEntity {
  public:
    virtual ~ScriptableEntity() { }

    template<typename T>
    T& GetComponent() {
      return m_Entity.GetComponent<T>();
    }

  protected:
    virtual void OnCreate() { }
    virtual void OnUpdate(float deltaTime) { }
    virtual void OnDestroy() { }

    Entity m_Entity;

  private:
    friend class Scene;
  };
}
