#pragma once

#include <filesystem>

#include "System.h"

namespace Ox {
struct ReloadSceneEvent {
  ReloadSceneEvent() = default;
};

class HotReloadableScenes : public System {
public:
  float update_interval = 1.0f;

  HotReloadableScenes(std::string scene_path) : System("HotReloadableScenes"), m_scene_path(std::move(scene_path)) { }

  void on_init() override;
  void on_update() override;
  void on_shutdown() override;

  void set_scene_path(const std::string& path);

private:
  std::string m_scene_path;
  std::filesystem::file_time_type m_last_write_time;
};
}
