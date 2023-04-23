#include "src/oxpch.h"
#include "MaterialSerializer.h"
#include "Material.h"
#include <fstream>

#include "AssetManager.h"
#include "Utils/FileUtils.h"
#include "Utils/Profiler.h"

namespace Oxylus {
  void MaterialSerializer::Serialize(const std::string& path) const {
    ZoneScoped;
    m_Material->Path = path;

    ryml::Tree tree;

    ryml::NodeRef nodeRoot = tree.rootref();
    nodeRoot |= ryml::MAP;

    nodeRoot["Material"] << m_Material->Name;

    auto parNode = nodeRoot["Parameters"];
    parNode |= ryml::MAP;

    const auto& Parameters = m_Material->Parameters;
    parNode["Roughness"] << Parameters.Roughness;
    parNode["Metallic"] << Parameters.Metallic;
    parNode["Specular"] << Parameters.Specular;
    parNode["Normal"] << Parameters.Normal;
    parNode["AO"] << Parameters.AO;
    auto colorNode = parNode["Color"];
    glm::write(&colorNode, Parameters.Color);
    parNode["UseAlbedo"] << Parameters.UseAlbedo;
    parNode["UseRoughness"] << Parameters.UseRoughness;
    parNode["UseMetallic"] << Parameters.UseMetallic;
    parNode["UseNormal"] << Parameters.UseNormal;
    parNode["UseAO"] << Parameters.UseAO;
    parNode["UseEmissive"] << Parameters.UseAO;
    parNode["UseSpecular"] << Parameters.UseSpecular;
    parNode["FlipImage"] << Parameters.FlipImage;
    parNode["DoubleSided"] << Parameters.DoubleSided;
    parNode["DoubleSided"] << Parameters.UVScale;

    auto textureNode = nodeRoot["Textures"];
    textureNode |= ryml::MAP;

    SaveIfPathExists(textureNode["Albedo"], m_Material->AlbedoTexture);
    SaveIfPathExists(textureNode["Normal"], m_Material->NormalTexture);
    SaveIfPathExists(textureNode["Roughness"], m_Material->RoughnessTexture);
    SaveIfPathExists(textureNode["Metallic"], m_Material->MetallicTexture);
    SaveIfPathExists(textureNode["AO"], m_Material->AOTexture);
    SaveIfPathExists(textureNode["Emissive"], m_Material->EmissiveTexture);

    std::stringstream ss;
    ss << tree;
    std::ofstream filestream(path);
    filestream << ss.str();
  }

  void MaterialSerializer::Deserialize(const std::string& path) const {
    if (path.empty())
      return;
    ZoneScoped;

    m_Material->Destroy();

    m_Material->Path = path;

    const auto& content = FileUtils::ReadFile(path);
    if (content.empty())
      return;

    ryml::Tree tree = ryml::parse_in_arena(c4::to_csubstr(content));

    const ryml::ConstNodeRef nodeRoot = tree.rootref();

    auto& Parameters = m_Material->Parameters;

    nodeRoot["Material"] >> m_Material->Name;

    const auto parNode = nodeRoot["Parameters"];
    TryLoad(parNode, "Roughness", Parameters.Roughness);
    TryLoad(parNode, "Metallic", Parameters.Metallic);
    TryLoad(parNode, "Specular", Parameters.Specular);
    TryLoad(parNode, "Normal", Parameters.Normal);
    TryLoad(parNode, "AO", Parameters.AO);
    glm::read(parNode["Color"], &Parameters.Color);
    TryLoad(parNode, "UseAlbedo", Parameters.UseAlbedo);
    TryLoad(parNode, "UseRoughness", Parameters.UseRoughness);
    TryLoad(parNode, "UseMetallic", Parameters.UseMetallic);
    TryLoad(parNode, "UseNormal", Parameters.UseNormal);
    TryLoad(parNode, "UseAO", Parameters.UseAO);
    TryLoad(parNode, "UseEmissive", Parameters.UseAO);
    TryLoad(parNode, "UseSpecular", Parameters.UseSpecular);
    TryLoad(parNode, "FlipImage", Parameters.FlipImage);
    TryLoad(parNode, "DoubleSided", Parameters.DoubleSided);
    TryLoad(parNode, "UVScale", Parameters.UVScale);

    VulkanImageDescription desc;
    desc.FlipOnLoad = true;
    desc.CreateDescriptorSet = true;

    LoadIfPathExists(nodeRoot["Textures"], "Albedo", m_Material->AlbedoTexture, desc);
    LoadIfPathExists(nodeRoot["Textures"], "Normal", m_Material->NormalTexture, desc);
    LoadIfPathExists(nodeRoot["Textures"], "Roughness", m_Material->RoughnessTexture, desc);
    LoadIfPathExists(nodeRoot["Textures"], "Metallic", m_Material->MetallicTexture, desc);
    LoadIfPathExists(nodeRoot["Textures"], "AO", m_Material->AOTexture, desc);
    LoadIfPathExists(nodeRoot["Textures"], "AO", m_Material->AOTexture, desc);
    LoadIfPathExists(nodeRoot["Textures"], "Emmisive", m_Material->EmissiveTexture, desc);

    m_Material->Update();
  }

  void MaterialSerializer::SaveIfPathExists(ryml::NodeRef node, const Ref<VulkanImage>& texture) const {
    if (!texture->GetDesc().Path.empty())
      node << texture->GetDesc().Path;
  }

  void MaterialSerializer::LoadIfPathExists(ryml::ConstNodeRef parentNode,
                                            const char* nodeName,
                                            Ref<VulkanImage>& texture,
                                            VulkanImageDescription& desc) const {
    std::string path{};
    if (parentNode.has_child(ryml::to_csubstr(nodeName))) {
      parentNode[ryml::to_csubstr(nodeName)] >> path;
      desc.Path = path;
      texture = AssetManager::GetImageAsset(desc).Data;
    }
  }
}
