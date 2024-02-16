#include "Project.h"

#include <entt/locator/locator.hpp>
#include <entt/meta/node.hpp>
#include <entt/meta/context.hpp>

#include "Base.h"
#include "ModuleManager.h"
#include "ProjectSerializer.h"

#include "Scripting/LuaManager.h"

#include "Utils/Log.h"
#include "Utils/Profiler.h"

namespace Ox {
void Project::load_module() {
  if (get_config().module_name.empty())
    return;
  const auto module_path = get_project_directory().append(project_config.module_name).string();
  const auto lib = ModuleManager::get()->add_lib(project_config.module_name, module_path);
  if (!lib)
    return;
  auto* state = LuaManager::get()->get_state();
  auto& ctx = entt::locator<entt::meta_ctx>::value_or();

  try {
    const auto func = lib->get_function<void(sol::state*, entt::meta_ctx*)>("bind_components");
    func(state, &ctx);
    OX_CORE_INFO("Successfully loaded module: {}", project_config.module_name);
  }
  catch (const std::exception& exc) {
    OX_CORE_ERROR("{}", exc.what());
  }
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

Shared<Project> Project::load(const std::filesystem::path& path) {
  const Shared<Project> project = create_shared<Project>();

  const ProjectSerializer serializer(project);
  if (serializer.deserialize(path)) {
    project->set_project_dir(path.parent_path().string());
    active_project = project;
    active_project->load_module();
    OX_CORE_INFO("Project loaded: {0}", project->get_config().name);
    return active_project;
  }

  return nullptr;
}

bool Project::save_active(const std::filesystem::path& path) {
  const ProjectSerializer serializer(active_project);
  if (serializer.serialize(path)) {
    active_project->set_project_dir(path.parent_path().string());
    return true;
  }
  return false;
}
}
