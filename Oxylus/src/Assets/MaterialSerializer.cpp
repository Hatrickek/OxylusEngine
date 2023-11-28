#include "MaterialSerializer.h"
#include "Material.h"
#include <fstream>

#include "AssetManager.h"
#include "Utils/FileUtils.h"
#include "Utils/Log.h"
#include "Utils/Profiler.h"

namespace Oxylus {
void MaterialSerializer::Serialize(const std::string& path) const {
  OX_SCOPED_ZONE;
  m_Material->path = path;

  ryml::Tree tree;

  ryml::NodeRef nodeRoot = tree.rootref();
  nodeRoot |= ryml::MAP;

  nodeRoot["Material"] << m_Material->name;

  auto parNode = nodeRoot["Parameters"];
  parNode |= ryml::MAP;

  const auto& Parameters = m_Material->parameters;
  parNode["Roughness"] << Parameters.roughness;
  parNode["Metallic"] << Parameters.metallic;
  parNode["Specular"] << Parameters.specular;
  parNode["Normal"] << Parameters.normal;
  parNode["AO"] << Parameters.ao;
  auto colorNode = parNode["Color"];
  glm::write(&colorNode, Parameters.color);
  parNode["UseAlbedo"] << Parameters.use_albedo;
  parNode["UsePhysicalMap"] << Parameters.use_physical_map;
  parNode["UseNormal"] << Parameters.use_normal;
  parNode["UseAO"] << Parameters.use_ao;
  parNode["UseEmissive"] << Parameters.use_ao;
  parNode["UseSpecular"] << Parameters.use_specular;
  parNode["DoubleSided"] << Parameters.double_sided;
  parNode["UVScale"] << Parameters.uv_scale;

  auto textureNode = nodeRoot["Textures"];
  textureNode |= ryml::MAP;

  SaveIfPathExists(textureNode["Albedo"], m_Material->albedo_texture);
  SaveIfPathExists(textureNode["Normal"], m_Material->normal_texture);
  SaveIfPathExists(textureNode["Physical"], m_Material->metallic_roughness_texture);
  SaveIfPathExists(textureNode["AO"], m_Material->ao_texture);
  SaveIfPathExists(textureNode["Emissive"], m_Material->emissive_texture);

  std::stringstream ss;
  ss << tree;
  std::ofstream filestream(path);
  filestream << ss.str();
}

void MaterialSerializer::Deserialize(const std::string& path) const {
  if (path.empty())
    return;
  OX_SCOPED_ZONE;

  m_Material->destroy();

  m_Material->path = path;

  auto content = FileUtils::read_file(path);
  if (content.empty()) {
    OX_CORE_ERROR("Couldn't read material file: {0}", path);

    // Try to read it again from assets path
    content = FileUtils::read_file(AssetManager::get_asset_file_system_path(path).string());
    if (!content.empty())
      OX_CORE_INFO("Could load the material file from assets path: {0}", path);
    else
      return;
  }

  ryml::Tree tree = ryml::parse_in_arena(c4::to_csubstr(content));

  const ryml::ConstNodeRef nodeRoot = tree.rootref();

  auto& Parameters = m_Material->parameters;

  nodeRoot["Material"] >> m_Material->name;

  const auto parNode = nodeRoot["Parameters"];
  TryLoad(parNode, "Roughness", Parameters.roughness);
  TryLoad(parNode, "Metallic", Parameters.metallic);
  TryLoad(parNode, "Specular", Parameters.specular);
  TryLoad(parNode, "Normal", Parameters.normal);
  TryLoad(parNode, "AO", Parameters.ao);
  glm::read(parNode["Color"], &Parameters.color);
  TryLoad(parNode, "UseAlbedo", Parameters.use_albedo);
  TryLoad(parNode, "UsePhysicalMap", Parameters.use_physical_map);
  TryLoad(parNode, "UseNormal", Parameters.use_normal);
  TryLoad(parNode, "UseAO", Parameters.use_ao);
  TryLoad(parNode, "UseEmissive", Parameters.use_ao);
  TryLoad(parNode, "UseSpecular", Parameters.use_specular);
  TryLoad(parNode, "DoubleSided", Parameters.double_sided);
  TryLoad(parNode, "UVScale", Parameters.uv_scale);

  LoadIfPathExists(nodeRoot["Textures"], "Albedo", m_Material->albedo_texture);
  LoadIfPathExists(nodeRoot["Textures"], "Normal", m_Material->normal_texture);
  LoadIfPathExists(nodeRoot["Textures"], "Physical", m_Material->metallic_roughness_texture);
  LoadIfPathExists(nodeRoot["Textures"], "AO", m_Material->ao_texture);
  LoadIfPathExists(nodeRoot["Textures"], "Emmisive", m_Material->emissive_texture);
}

void MaterialSerializer::SaveIfPathExists(ryml::NodeRef node, const Ref<TextureAsset>& texture) const {
  if (!texture->get_path().empty())
    node << texture->get_path();
}

void MaterialSerializer::LoadIfPathExists(ryml::ConstNodeRef parentNode,
                                          const char* nodeName,
                                          Ref<TextureAsset>& texture) const {
  std::string path{};
  if (parentNode.has_child(ryml::to_csubstr(nodeName))) {
    parentNode[ryml::to_csubstr(nodeName)] >> path;
    texture = AssetManager::get_texture_asset({path});
  }
}
}
