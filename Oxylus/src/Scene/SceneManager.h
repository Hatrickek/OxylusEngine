#pragma once

#include "Scene.h"

namespace Oxylus {
  class SceneManager {
  public:
    static Ref<Scene> GetActiveScene();

    static void SetActiveScene(const Ref<Scene>& scene);

    static Ref<Scene> m_ActiveScene;
  };
}
