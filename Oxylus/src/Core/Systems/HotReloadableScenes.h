#pragma once
#include <filesystem>

#include "System.h"
#include <Event/Event.h>

namespace Oxylus {
  struct ReloadSceneEvent {
    ReloadSceneEvent() = default;
  };

  class HotReloadableScenes : public System {
  public:
    float UpdateInterval = 1.0f;

    HotReloadableScenes(std::string scenePath) : System("HotReloadableScenes"), m_ScenePath(std::move(scenePath)) {}

    void OnInit() override;
    void OnUpdate() override;
    void OnShutdown() override;

    void SetScenePath(const std::string& path);
  private:
    std::string m_ScenePath;
    std::filesystem::file_time_type m_LastWriteTime;
  };
}
