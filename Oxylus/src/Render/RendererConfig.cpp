#include "RendererConfig.h"

#include "Core/YamlHelpers.h"
#include "Utils/FileUtils.h"

#include <fstream>
#include <sstream>

namespace Oxylus {
RendererConfig* RendererConfig::s_Instance = nullptr;

RendererConfig::RendererConfig() {
  s_Instance = this;
}

void RendererConfig::SaveConfig(const char* path) const {
  ryml::Tree tree;

  ryml::NodeRef nodeRoot = tree.rootref();
  nodeRoot |= ryml::MAP;

  //TODO: Add comment at the top of the file: "# Renderer settings" (Note: Couldn't find how to add comments in ryml docs)

  //Display
  {
    auto node = nodeRoot["Display"];
    node |= ryml::MAP;

    node["VSync"] << DisplayConfig.VSync;
  }

  //Color
  {
    auto node = nodeRoot["Color"];
    node |= ryml::MAP;

    node["Tonemapper"] << ColorConfig.Tonemapper;
    node["Exposure"] << ColorConfig.Exposure;
    node["Gamma"] << ColorConfig.Gamma;
  }

  //SSAO
  {
    auto node = nodeRoot["SSAO"];
    node |= ryml::MAP;

    node["Enabled"] << SSAOConfig.Enabled;
    node["Radius"] << SSAOConfig.Radius;
  }

  //Bloom
  {
    auto node = nodeRoot["Bloom"];
    node |= ryml::MAP;

    node["Enabled"] << BloomConfig.Enabled;
    node["Threshold"] << BloomConfig.Threshold;
  }

  //SSR
  {
    auto node = nodeRoot["SSR"];
    node |= ryml::MAP;

    node["Enabled"] << SSRConfig.Enabled;
  }

  //DirectShadows
  {
    auto node = nodeRoot["DirectShadows"];
    node |= ryml::MAP;

    node["Enabled"] << DirectShadowsConfig.Enabled;
    node["Quality"] << DirectShadowsConfig.Quality;
    node["UsePCF"] << DirectShadowsConfig.UsePCF;
  }

  std::stringstream ss;
  ss << tree;
  std::ofstream filestream(path);
  filestream << ss.str();
}

bool RendererConfig::LoadConfig(const char* path) {
  const auto& content = FileUtils::ReadFile(path);
  if (!content)
    return false;

  ryml::Tree tree = ryml::parse_in_arena(c4::to_csubstr(*content));

  const ryml::ConstNodeRef root = tree.rootref();

  const ryml::ConstNodeRef nodeRoot = root;

  //Display
  {
    const ryml::ConstNodeRef node = nodeRoot["Display"];
    node["VSync"] >> DisplayConfig.VSync;
  }

  //Color
  {
    const ryml::ConstNodeRef node = nodeRoot["Color"];
    node["Tonemapper"] >> ColorConfig.Tonemapper;
    node["Exposure"] >> ColorConfig.Exposure;
    node["Gamma"] >> ColorConfig.Gamma;
  }

  //SSAO
  {
    const ryml::ConstNodeRef node = nodeRoot["SSAO"];

    node["Enabled"] >> SSAOConfig.Enabled;
    node["Radius"] >> SSAOConfig.Radius;
  }

  //Bloom
  {
    const ryml::ConstNodeRef node = nodeRoot["Bloom"];

    node["Enabled"] >> BloomConfig.Enabled;
    node["Threshold"] >> BloomConfig.Threshold;
  }

  //SSR
  {
    const ryml::ConstNodeRef node = nodeRoot["SSR"];

    node["Enabled"] >> SSRConfig.Enabled;
  }

  //DirectShadows
  {
    const ryml::ConstNodeRef node = nodeRoot["DirectShadows"];

    node["Enabled"] >> DirectShadowsConfig.Enabled;
    node["Quality"] >> DirectShadowsConfig.Quality;
    node["UsePCF"] >> DirectShadowsConfig.UsePCF;
  }

  return true;
}
}
