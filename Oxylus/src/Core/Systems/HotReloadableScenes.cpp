#include "HotReloadableScenes.h"
#include "Utils/Log.h"

#include <chrono>
#include <fstream>

namespace Oxylus {
void HotReloadableScenes::on_init() {
  if (!std::filesystem::exists(m_scene_path)) {
    OX_CORE_ERROR("System HotReloadableScene: Scene path doesn't exist: {0}", m_scene_path);
    return;
  }
  m_last_write_time = std::filesystem::last_write_time(m_scene_path);
}

void HotReloadableScenes::on_update() {
  using namespace std::filesystem;
  if (last_write_time(m_scene_path).time_since_epoch().count()
      != m_last_write_time.time_since_epoch().count()) {
    //File changed event
    m_dispatcher->trigger<ReloadSceneEvent>();
    m_last_write_time = last_write_time(m_scene_path);
  }
}

void HotReloadableScenes::on_shutdown() { }

void HotReloadableScenes::set_scene_path(const std::string& path) {
  m_scene_path = path;
}
}
