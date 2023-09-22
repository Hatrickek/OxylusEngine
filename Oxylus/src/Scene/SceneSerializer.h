#pragma once

#include "Scene.h"

namespace Oxylus {
class SceneSerializer {
public:
  SceneSerializer(const Ref<Scene>& scene);

  void Serialize(const std::string& filePath) const;
  //void SerializeRuntime(const std::string& filePath);

  bool Deserialize(const std::string& filePath) const;
  //bool DeserializeRuntime(const std::string& filePath);
private:
  Ref<Scene> m_Scene;
};
}
