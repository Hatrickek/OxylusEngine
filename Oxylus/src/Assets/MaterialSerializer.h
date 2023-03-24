#pragma once
#include "Material.h"
#include "Core/YamlHelpers.h"

namespace Oxylus {
  class MaterialSerializer {
  public:
    MaterialSerializer(Material& material) : m_Material(&material) { }

    void Serialize(const std::string& path) const;
    void Deserialize(const std::string& path) const;

  private:
    void SaveIfPathExists(ryml::NodeRef node, const Ref<VulkanImage>& texture) const;
    void LoadIfPathExists(ryml::ConstNodeRef parentNode,
                          const char* nodeName,
                          Ref<VulkanImage>& texture,
                          VulkanImageDescription& desc) const;
    Material* m_Material;
  };
}
