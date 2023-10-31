#pragma once

#include <filesystem>
#include <string>

#include "Base.h"

namespace Oxylus {
struct ProjectConfig {
  std::string name = "Untitled";

  std::string start_scene;
  std::string asset_directory;
};

class Project {
public:
  ProjectConfig& get_config() { return m_config; }

  static std::filesystem::path get_project_directory() { return s_active_project->m_project_directory; }

  static std::filesystem::path get_asset_directory() {
    return get_project_directory() / s_active_project->m_config.asset_directory;
  }

  static Ref<Project> get_active() { return s_active_project; }
  static Ref<Project> create_new();
  static Ref<Project> load(const std::filesystem::path& path);
  static bool save_active(const std::filesystem::path& path);

private:
  ProjectConfig m_config;
  std::filesystem::path m_project_directory;
  inline static Ref<Project> s_active_project;
};
}
