#include "ProjectSerializer.h"

#include "Core/YamlHelpers.h"
#include <Utils/FileUtils.h>
#include "Utils/Log.h"

#include <fstream>


namespace Oxylus {
ProjectSerializer::ProjectSerializer(Ref<Project> project) : m_project(std::move(project)) { }

bool ProjectSerializer::serialize(const std::filesystem::path& file_path) const {
  const auto& config = m_project->get_config();

  ryml::Tree tree;

  ryml::NodeRef nodeRoot = tree.rootref();
  nodeRoot |= ryml::MAP;

  auto node = nodeRoot["Project"];
  node |= ryml::MAP;

  node["Name"] << config.name;
  node["StartScene"] << config.start_scene;
  node["AssetDirectory"] << config.asset_directory;

  std::stringstream ss;
  ss << tree;
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

  ryml::Tree tree = ryml::parse_in_arena(c4::to_csubstr(content));

  const ryml::ConstNodeRef root = tree.rootref();

  if (!root.has_child("Project"))
    return false;

  const ryml::ConstNodeRef nodeRoot = root["Project"];

  nodeRoot["Name"] >> Name;
  nodeRoot["StartScene"] >> StartScene;
  nodeRoot["AssetDirectory"] >> AssetDirectory;

  m_project->set_project_file_path(file_path.string());

  return true;
}
}
