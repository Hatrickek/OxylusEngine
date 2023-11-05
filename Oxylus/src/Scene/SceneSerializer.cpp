#include "SceneSerializer.h"

#include <fstream>
#include "Core/Entity.h"
#include "Core/Project.h"
#include "Core/YamlHelpers.h"
#include "Utils/Profiler.h"
#include "Utils/FileUtils.h"

#include "EntitySerializer.h"
#include "Assets/AssetManager.h"

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

  for (const auto [e] : m_Scene->m_registry.storage<entt::entity>().each()) {
    const Entity entity = {e, m_Scene.get()};
    if (!entity)
      return;

    EntitySerializer::SerializeEntity(m_Scene.get(), entities, entity);
  }

  std::stringstream ss;
  ss << tree;
  std::ofstream filestream(filePath);
  filestream << ss.str();

  OX_CORE_INFO("Saved scene {0}.", m_Scene->scene_name);
}

bool SceneSerializer::deserialize(const std::string& filePath) const {
  ProfilerTimer timer("Scene serializer");

  auto content = FileUtils::read_file(filePath);
  if (content.empty()) {
    OX_CORE_ASSERT(!content.empty(), fmt::format("Couldn't read scene file: {0}", filePath).c_str());

    // Try to read it again from assets path
    content = FileUtils::read_file(AssetManager::get_asset_file_system_path(filePath).string());
    if (!content.empty())
      OX_CORE_INFO("Could load the file from assets path: {0}", filePath);
    else {
      return false;
    }
  }

  ryml::Tree tree = ryml::parse_in_arena(ryml::to_csubstr(content));

  if (tree.empty()) {
    OX_CORE_ERROR("Scene was unable to load from YAML file {0}", filePath);
    return false;
  }

  const ryml::ConstNodeRef root = tree.rootref();

  if (!root.has_child("Scene"))
    return false;

  root["Scene"] >> m_Scene->scene_name;

  if (root.has_child("Entities")) {
    const ryml::ConstNodeRef entities = root["Entities"];

    for (const auto entity : entities) {
      EntitySerializer::DeserializeEntity(entity, m_Scene.get(), true);
    }

    timer.Stop();
    OX_CORE_INFO("Scene loaded : {0}, {1} ms", FileSystem::get_file_name(m_Scene->scene_name), timer.ElapsedMilliSeconds());
    return true;
  }

  OX_CORE_ERROR("Scene doesn't contain any entities! {0}", FileSystem::get_file_name(m_Scene->scene_name));
  return false;
}
}
