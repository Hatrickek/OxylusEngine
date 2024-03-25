#include "Project.hpp"

#include <entt/locator/locator.hpp>
#include <entt/meta/context.hpp>
#include <entt/meta/node.hpp>

#include "Modules/ModuleRegistry.hpp"
#include "Base.hpp"
#include "FileSystem.hpp"
#include "ProjectSerializer.hpp"

#include "Modules/ModuleInterface.hpp"
#include "Modules/ModuleUtil.hpp"

#include "Utils/Log.hpp"
#include "Utils/Profiler.hpp"

namespace ox {
std::string Project::get_asset_directory() {
  return FileSystem::append_paths(get_project_directory(), active_project->project_config.asset_directory);
}

void Project::load_module() {
  if (get_config().module_name.empty())
    return;

  const auto module_path = FileSystem::append_paths(get_project_directory(), project_config.module_name);
  ModuleUtil::load_module(project_config.module_name, module_path);
}

void Project::unload_module() const {
  if (!project_config.module_name.empty())
    ModuleUtil::unload_module(project_config.module_name);
}

Shared<Project> Project::create_new() {
  OX_SCOPED_ZONE;
  active_project = create_shared<Project>();
  return active_project;
}

Shared<Project> Project::new_project(const std::string& project_dir, const std::string& project_name, const std::string& project_asset_dir) {
  auto project = create_shared<Project>();
  project->get_config().name = project_name;
  project->get_config().asset_directory = project_asset_dir;

  project->set_project_dir(project_dir);

  if (project_dir.empty())
    return nullptr;

  std::filesystem::create_directory(project_dir);

  const auto asset_folder_dir = FileSystem::append_paths(project_dir, project_asset_dir);
  std::filesystem::create_directory(asset_folder_dir);

  const ProjectSerializer serializer(project);
  serializer.serialize(FileSystem::append_paths(project_dir, project_name + ".oxproj"));

  set_active(project);

  return project;
}

Shared<Project> Project::load(const std::string& path) {
  const Shared<Project> project = create_shared<Project>();

  const ProjectSerializer serializer(project);
  if (serializer.deserialize(path)) {
    project->set_project_dir(std::filesystem::path(path).parent_path().string());
    active_project = project;
    active_project->load_module();
    OX_LOG_INFO("Project loaded: {0}", project->get_config().name);
    return active_project;
  }

  return nullptr;
}

bool Project::save_active(const std::string& path) {
  const ProjectSerializer serializer(active_project);
  if (serializer.serialize(path)) {
    active_project->set_project_dir(std::filesystem::path(path).parent_path().string());
    return true;
  }
  return false;
}
}
