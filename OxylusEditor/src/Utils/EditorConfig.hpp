#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <Utils/CVars.hpp>

namespace ox {
class Project;

namespace EditorCVar {
inline AutoCVar_Float cvar_camera_speed("editor.camera_speed", "editor camera speed", 5.0f);
inline AutoCVar_Float cvar_camera_sens("editor.camera_sens", "editor camera sens", 0.1f);
inline AutoCVar_Int cvar_camera_smooth("editor.camera_smooth", "editor camera smoothing", 1);
inline AutoCVar_Int cvar_file_thumbnails("editor.file_thumbnails", "show file thumbnails in content panel", 1);
inline AutoCVar_Float cvar_file_thumbnail_size("editor.file_thumbnail_size", "file thumbnail size in content panel", 120.0f);
}

class EditorConfig {
public:
  EditorConfig();
  ~EditorConfig() = default;

  static EditorConfig* get() { return instance; }

  void load_config();
  void save_config() const;

  void add_recent_project(const Project* path);

  const std::vector<std::string>& get_recent_projects() const { return recent_projects; }

private:
  std::vector<std::string> recent_projects{};
  static EditorConfig* instance;
};
}
