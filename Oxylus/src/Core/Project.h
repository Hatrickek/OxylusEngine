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
  Project() = default;
  Project(const std::string& dir) : m_project_directory(dir) { }

  ProjectConfig& get_config() { return m_config; }

  static std::filesystem::path get_project_directory() { return s_active_project->m_project_directory; }
  void set_project_dir(const std::string& dir) { m_project_directory = dir; }
  static std::filesystem::path get_asset_directory() { return get_project_directory() / s_active_project->m_config.asset_directory; }
  const std::string& get_project_file_path() const { return project_file_path; }
  void set_project_file_path(const std::string& path) { project_file_path = path; }

  static Ref<Project> create_new();
  static void set_active(const Ref<Project>& project) { s_active_project = project; }
  static Ref<Project> get_active() { return s_active_project; }
  static Ref<Project> load(const std::filesystem::path& path);
  static bool save_active(const std::filesystem::path& path);

private:
  ProjectConfig m_config;
  std::filesystem::path m_project_directory;
  std::string project_file_path;
  inline static Ref<Project> s_active_project;
};
}
