#include "EditorConfig.h"
#include <fstream>

#include "EditorLayer.h"
#include "Core/Application.h"
#include "Utils/Log.h"
#include "Core/YamlHelpers.h"
#include "Utils/FileUtils.h"
#include "Utils/UIUtils.h"

namespace Oxylus {
  EditorConfig* EditorConfig::s_Instace = nullptr;

  constexpr const char* EditorConfigFileName = "editor.oxconfig";

  EditorConfig::EditorConfig() {
    if (!s_Instace)
      s_Instace = this;
  }

  void EditorConfig::LoadConfig() {
    const auto& content = FileUtils::read_file(EditorConfigFileName);
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
      m_RecentProjects.emplace_back(str);
    }
  }

  void EditorConfig::SaveConfig() const {
    ryml::Tree tree;

    ryml::NodeRef nodeRoot = tree.rootref();
    nodeRoot |= ryml::MAP;

    auto node = nodeRoot["EditorConfig"];
    node |= ryml::MAP;
    auto projectsNode = node["RecentProjects"];
    projectsNode |= ryml::SEQ;
    for (auto& project : m_RecentProjects) {
      projectsNode.append_child() << project;
    }

    std::stringstream ss;
    ss << tree;
    std::ofstream filestream(EditorConfigFileName);
    filestream << ss.str();
  }

  void EditorConfig::AddRecentProject(const std::string& path) {
    for (auto& project : m_RecentProjects) {
      if (project == path) {
        return;
      }
    }
    m_RecentProjects.emplace_back(path);
  }
}
