#include "ProjectSerializer.h"

#include <Utils/FileUtils.h>
#include "Utils/Log.h"
#include "Utils/Toml.h"

#include <fstream>

namespace Ox {
ProjectSerializer::ProjectSerializer(Shared<Project> project) : m_project(std::move(project)) {}

bool ProjectSerializer::serialize(const std::filesystem::path& file_path) const {
  const auto& config = m_project->get_config();

  const auto root = toml::table{
    {
      "project",
      toml::table{
        {"name", config.name},
        {"asset_directory", config.asset_directory},
        {"start_scene", config.start_scene},
      }
    }
  };

  std::stringstream ss;
  ss << "# Oxylus project file\n";
  ss << root;
  std::ofstream filestream(file_path);
  filestream << ss.str();

  m_project->set_project_file_path(file_path.string());

  return true;
}

bool ProjectSerializer::deserialize(const std::filesystem::path& file_path) const {
  auto& [Name, StartScene, AssetDirectory] = m_project->get_config();

  const auto& content = FileUtils::read_file(file_path.string());
  if (content.empty()) {
    OX_CORE_ASSERT(!content.empty(), fmt::format("Couldn't load project file: {0}", file_path.string()).c_str());
    return false;
  }

  toml::table toml = toml::parse(content);

  const auto project_node = toml["project"];
  Name = project_node["name"].as_string()->get(); 
  AssetDirectory = project_node["asset_directory"].as_string()->get(); 
  StartScene = project_node["start_scene"].as_string()->get(); 

  m_project->set_project_file_path(file_path.string());

  return true;
}
}
