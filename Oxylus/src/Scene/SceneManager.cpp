#include "src/oxpch.h"
#include "SceneManager.h"

namespace Oxylus {
  Ref <Scene> SceneManager::m_ActiveScene = nullptr;

  Ref <Scene> SceneManager::GetActiveScene() {
    return m_ActiveScene;
  }

  void SceneManager::SetActiveScene(Ref <Scene> scene) {
    m_ActiveScene = scene;
  }
}