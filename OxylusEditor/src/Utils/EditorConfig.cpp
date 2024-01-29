#include "EditorConfig.h"
#include <fstream>

#include <Render/RendererConfig.h>

#include "EditorLayer.h"
#include "Core/Application.h"
#include "Core/Project.h"

#include "Utils/Log.h"
#include "Core/YamlHelpers.h"
#include "Utils/FileUtils.h"
#include "Utils/UIUtils.h"

#include <toml++/toml.hpp>

namespace Oxylus {
EditorConfig* EditorConfig::instance = nullptr;

constexpr const char* EDITOR_CONFIG_FILE_NAME = "editor_config.toml";

EditorConfig::EditorConfig() {
  if (!instance)
    instance = this;
}

void EditorConfig::load_config() {
  OX_SCOPED_ZONE;
  const auto& content = FileUtils::read_file(EDITOR_CONFIG_FILE_NAME);
  if (content.empty())
    return;

  toml::table toml = toml::parse(content);
  const auto config = toml["editor_config"];
  for (auto& project : *config["recent_projects"].as_array()) {
    recent_projects.emplace_back(*project.as_string());
  }
  RendererCVar::cvar_draw_grid.set(config["grid"].as_boolean()->get());
  RendererCVar::cvar_draw_grid_distance.set((float)config["grid_distance"].as_floating_point()->get());
  EditorCVar::cvar_camera_speed.set((float)config["camera_speed"].as_floating_point()->get());
  EditorCVar::cvar_camera_sens.set((float)config["camera_sens"].as_floating_point()->get());
  EditorCVar::cvar_camera_smooth.set(config["camera_smooth"].as_boolean()->get());
}

void EditorConfig::save_config() const {
  toml::array recent_projects_array;

  for (auto& project : recent_projects)
    recent_projects_array.emplace_back(project);

  const auto root = toml::table{
    {
      "editor_config",
      toml::table{
        {"recent_projects", recent_projects_array},
        {"grid", (bool)RendererCVar::cvar_draw_grid.get()},
        {"grid_distance", RendererCVar::cvar_draw_grid_distance.get()},
        {"camera_speed", EditorCVar::cvar_camera_speed.get()},
        {"camera_sens", EditorCVar::cvar_camera_sens.get()},
        {"camera_smooth", (bool)EditorCVar::cvar_camera_smooth.get()},
      }
    }
  };

  std::stringstream ss;
  ss << root;
  std::ofstream filestream(EDITOR_CONFIG_FILE_NAME);
  filestream << ss.str();
}

void EditorConfig::add_recent_project(const Project* path) {
  for (auto& project : recent_projects) {
    if (project == path->get_project_file_path()) {
      return;
    }
  }
  recent_projects.emplace_back(path->get_project_file_path());
}
}
