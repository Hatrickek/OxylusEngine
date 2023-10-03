#include "ProjectSerializer.h"

#include "Core/YamlHelpers.h"
#include <Utils/FileUtils.h>
#include "Utils/Log.h"

#include <fstream>


namespace Oxylus {
ProjectSerializer::ProjectSerializer(Ref<Project> project) : m_Project(std::move(project)) { }

bool ProjectSerializer::Serialize(const std::filesystem::path& filePath) const {
  const auto& config = m_Project->GetConfig();

  ryml::Tree tree;

  ryml::NodeRef root = tree.rootref();
  root |= ryml::MAP;
  auto node = root["Project"];

  node["Name"] << config.Name;
  node["StartScene"] << config.StartScene;
  node["AssetDirectory"] << config.AssetDirectory;

  std::stringstream ss;
  ss << tree;
  std::ofstream filestream(filePath);
  filestream << ss.str();

  return true;
}

bool ProjectSerializer::Deserialize(const std::filesystem::path& filePath) const {
  auto& [Name, StartScene, AssetDirectory] = m_Project->GetConfig();

  const auto& content = FileUtils::read_file(filePath.string());
  if (content.empty()) {
    OX_CORE_ASSERT(!content.empty(), fmt::format("Couldn't load project file: {0}", filePath.string()).c_str());
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

  return true;
}
}
