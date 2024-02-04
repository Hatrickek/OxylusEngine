#pragma once

#include "Material.h"

namespace Ox {
class MaterialSerializer {
public:
  MaterialSerializer(Material& material) : m_material(&material) { }
  MaterialSerializer(const Shared<Material>& material) : m_material(material.get()) { }

  void serialize(const std::string& path) const;
  void deserialize(const std::string& path) const;

private:
#if 0
  static void save_if_path_exists(ryml::NodeRef node, const Ref<TextureAsset>& texture);
  static void load_if_path_exists(ryml::ConstNodeRef parent_node,
                                  const char* node_name,
                                  Ref<TextureAsset>& texture);
#endif

  Material* m_material;
};
}
