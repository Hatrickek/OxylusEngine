#include "EditorConfig.h"
#include <fstream>

#include "EditorLayer.h"
#include "Core/Application.h"
#include "Core/Project.h"

#include "Utils/Log.h"
#include "Core/YamlHelpers.h"
#include "Utils/FileUtils.h"
#include "Utils/UIUtils.h"

namespace Oxylus {
EditorConfig* EditorConfig::instance = nullptr;

constexpr const char* EDITOR_CONFIG_FILE_NAME = "editor.oxconfig";

EditorConfig::EditorConfig() {
  if (!instance)
    instance = this;
}

void EditorConfig::load_config() {
  OX_SCOPED_ZONE;
  const auto& content = FileUtils::read_file(EDITOR_CONFIG_FILE_NAME);
  if (content.empty())
    return;

  ryml::Tree tree = ryml::parse_in_arena(c4::to_csubstr(content));

  const ryml::ConstNodeRef root = tree.rootref();

  const ryml::ConstNodeRef nodeRoot = root;

  const ryml::ConstNodeRef node = nodeRoot["EditorConfig"];

  const auto projectsNode = node["RecentProjects"];
  for (size_t i = 0; i < projectsNode.num_children(); i++) {
    std::string str{};
    projectsNode[i] >> str;
    recent_projects.emplace_back(str);
  }
}

void EditorConfig::save_config() const {
  ryml::Tree tree;

  ryml::NodeRef nodeRoot = tree.rootref();
  nodeRoot |= ryml::MAP;

  auto node = nodeRoot["EditorConfig"];
  node |= ryml::MAP;
  auto projectsNode = node["RecentProjects"];
  projectsNode |= ryml::SEQ;
  for (auto& project : recent_projects) {
    projectsNode.append_child() << project;
  }

  std::stringstream ss;
  ss << tree;
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
