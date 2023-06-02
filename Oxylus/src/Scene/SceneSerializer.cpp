#include "src/oxpch.h"
#include "SceneSerializer.h"

#include <fstream>
#include "Core/Entity.h"
#include "Core/Project.h"
#include "Core/YamlHelpers.h"
#include "Utils/Profiler.h"
#include "Utils/FileUtils.h"

#include "EntitySerializer.h"
#include "Assets/AssetManager.h"
#include "Utils/StringUtils.h"

namespace Oxylus {
  SceneSerializer::SceneSerializer(const Ref<Scene>& scene) : m_Scene(scene) { }

  void SceneSerializer::Serialize(const std::string& filePath) const {
    ryml::Tree tree;

    ryml::NodeRef root = tree.rootref();
    root |= ryml::MAP;

    root["Scene"] << std::filesystem::path(filePath).filename().string();

    // Entities
    ryml::NodeRef entities = root["Entities"];
    entities |= ryml::SEQ;

    m_Scene->m_Registry.each([&](auto entityID) {
      const Entity entity = {entityID, m_Scene.get()};
      if (!entity)
        return;

      EntitySerializer::SerializeEntity(m_Scene.get(), entities, entity);
    });

    std::stringstream ss;
    ss << tree;
    std::ofstream filestream(filePath);
    filestream << ss.str();

    OX_CORE_INFO("Saved scene {0}.", m_Scene->SceneName);
  }

  bool SceneSerializer::Deserialize(const std::string& filePath) const {
    ProfilerTimer timer("Scene serializer");

    auto content = FileUtils::ReadFile(filePath);
    if (!content) {
      OX_CORE_ASSERT(content, fmt::format("Couldn't read scene file: {0}", filePath).c_str());

      // Try to read it again from assets path
      content = FileUtils::ReadFile(AssetManager::GetAssetFileSystemPath(filePath).string());
      if (content)
        OX_CORE_INFO("Could load the file from assets path: {0}", filePath);
      else {
        return false;
      }
    }

    ryml::Tree tree = ryml::parse_in_arena(ryml::to_csubstr(content.value()));

    if (tree.empty()) {
      OX_CORE_ERROR("Scene was unable to load from YAML file {0}", filePath);
      return false;
    }

    const ryml::ConstNodeRef root = tree.rootref();

    if (!root.has_child("Scene"))
      return false;

    root["Scene"] >> m_Scene->SceneName;

    if (root.has_child("Entities")) {
      const ryml::ConstNodeRef entities = root["Entities"];

      for (const auto entity : entities) {
        EntitySerializer::DeserializeEntity(entity, m_Scene.get(), true);
      }

      timer.Stop();
      OX_CORE_INFO("Scene loaded : {0}, {1} ms", StringUtils::GetName(m_Scene->SceneName), timer.ElapsedMilliSeconds());
      return true;
    }

    OX_CORE_BERROR("Scene doesn't contain any entities! {0}", StringUtils::GetName(m_Scene->SceneName));
    return false;
  }
}
