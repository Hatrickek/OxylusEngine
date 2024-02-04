#pragma once

#include "Scene.h"

namespace Ox {
class SceneSerializer {
public:
  SceneSerializer(const Shared<Scene>& scene);

  void serialize(const std::string& filePath) const;
  //void SerializeRuntime(const std::string& filePath);

  bool deserialize(const std::string& filePath) const;
  //bool DeserializeRuntime(const std::string& filePath);
private:
  Shared<Scene> m_scene;
};
}
