#include "HotReloadableScenes.h"
#include "Utils/Log.h"

#include <chrono>
#include <fstream>

namespace Oxylus {
void HotReloadableScenes::on_init() {
  if (!std::filesystem::exists(m_ScenePath)) {
    OX_CORE_ERROR("System HotReloadableScene: Scene path doesn't exist: {0}", m_ScenePath);
    return;
  }
  m_LastWriteTime = std::filesystem::last_write_time(m_ScenePath);
}

void HotReloadableScenes::on_update() {
  using namespace std::filesystem;
  if (last_write_time(m_ScenePath).time_since_epoch().count()
      != m_LastWriteTime.time_since_epoch().count()) {
    //File changed event
    m_Dispatcher->trigger<ReloadSceneEvent>();
    m_LastWriteTime = last_write_time(m_ScenePath);
  }
}

void HotReloadableScenes::on_shutdown() { }

void HotReloadableScenes::SetScenePath(const std::string& path) {
  m_ScenePath = path;
}
}
