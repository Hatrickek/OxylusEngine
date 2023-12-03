#include "Project.h"

#include "Base.h"
#include "ProjectSerializer.h"
#include "Utils/Log.h"

namespace Oxylus {
Ref<Project> Project::create_new() {
  s_active_project = create_ref<Project>();
  return s_active_project;
}

Ref<Project> Project::new_project(const std::string& project_dir, const std::string& project_name, const std::string& project_asset_dir) {
  const auto project = create_ref<Project>();
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

Ref<Project> Project::load(const std::filesystem::path& path) {
  const Ref<Project> project = create_ref<Project>();

  const ProjectSerializer serializer(project);
  if (serializer.deserialize(path)) {
    project->m_project_directory = path.parent_path();
    s_active_project = project;
    OX_CORE_INFO("Project loaded: {0}", project->get_config().name);
    return s_active_project;
  }

  return nullptr;
}

bool Project::save_active(const std::filesystem::path& path) {
  const ProjectSerializer serializer(s_active_project);
  if (serializer.serialize(path)) {
    s_active_project->m_project_directory = path.parent_path();
    return true;
  }
  return false;
}
}
