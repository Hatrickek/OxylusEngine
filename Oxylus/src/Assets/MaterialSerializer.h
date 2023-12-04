#pragma once

#include "Material.h"
#include "Core/YamlHelpers.h"

namespace Oxylus {
class MaterialSerializer {
public:
  MaterialSerializer(Material& material) : m_material(&material) { }
  MaterialSerializer(const Ref<Material>& material) : m_material(material.get()) { }

  void serialize(const std::string& path) const;
  void deserialize(const std::string& path) const;

private:
  static void save_if_path_exists(ryml::NodeRef node, const Ref<TextureAsset>& texture);
  static void load_if_path_exists(ryml::ConstNodeRef parent_node,
                                  const char* node_name,
                                  Ref<TextureAsset>& texture);

  Material* m_material;
};
}
