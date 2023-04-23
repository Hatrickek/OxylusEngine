#pragma once
#include "Event/Event.h"

namespace Oxylus {
  class Scene;

  class SceneRenderer {
  public:
    struct ProbeChangeEvent {};

    EventDispatcher Dispatcher;

    SceneRenderer() = default;
    ~SceneRenderer() = default;

    void Init(Scene& scene);
    void Render() const;

  private:
    Scene* m_Scene = nullptr;

    //Update probes with events
    void UpdateProbes() const;
  };
}
