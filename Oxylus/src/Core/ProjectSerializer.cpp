#include "ProjectSerializer.h"

#include <Utils/FileUtils.h>
#include "Utils/Log.h"
#include "Utils/Toml.h"

#include <fstream>
#include <sstream>

namespace ox {
ProjectSerializer::ProjectSerializer(Shared<Project> project) : project(std::move(project)) {}

bool ProjectSerializer::serialize(const std::string& file_path) const {
  const auto& config = project->get_config();

  const auto root = toml::table{
    {
      "project",
      toml::table{
        {"name", config.name},
        {"asset_directory", config.asset_directory},
        {"start_scene", config.start_scene},
        {"module_name", config.module_name}
      }
    }
  };

  std::stringstream ss;
  ss << "# Oxylus project file\n";
  ss << root;
  std::ofstream filestream(file_path);
  filestream << ss.str();

  project->set_project_file_path(file_path);

  return true;
}

bool ProjectSerializer::deserialize(std::string file_path) const {
  auto& [name, start_scene, asset_directory, module_name] = project->get_config();
  const auto content = FileUtils::read_file(file_path);
  if (content.empty()) {
    OX_CORE_ASSERT(!content.empty(), fmt::format("Couldn't load project file: {0}", file_path).c_str());
    return false;
  }

  try {
    toml::table toml = toml::parse(content);

    const auto project_node = toml["project"];
    name = project_node["name"].as_string()->get();
    asset_directory = project_node["asset_directory"].as_string()->get();
    start_scene = project_node["start_scene"].as_string()->get();
    module_name = project_node["module_name"].as_string()->get();
  }
  catch (const std::exception& exc) {
    OX_CORE_ERROR("{}", exc.what());
  }

  project->set_project_file_path(file_path);

  return true;
}
}
