#pragma once

#include "Scene.h"

namespace Oxylus {
class SceneSerializer {
public:
  SceneSerializer(const Ref<Scene>& scene);

  void serialize(const std::string& filePath) const;
  //void SerializeRuntime(const std::string& filePath);

  bool deserialize(const std::string& filePath) const;
  //bool DeserializeRuntime(const std::string& filePath);
private:
  Ref<Scene> m_scene;
};
}
