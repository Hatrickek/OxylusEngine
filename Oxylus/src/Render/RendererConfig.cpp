#include "RendererConfig.h"

#include "Core/YamlHelpers.h"
#include "Utils/FileUtils.h"

#include <fstream>
#include <sstream>

namespace Oxylus {
RendererConfig* RendererConfig::s_instance = nullptr;

RendererConfig::RendererConfig() {
  s_instance = this;
}

void RendererConfig::save_config(const char* path) const {
  ryml::Tree tree;

  ryml::NodeRef node_root = tree.rootref();
  node_root |= ryml::MAP;

  //TODO: Add comment at the top of the file: "# Renderer settings" (Note: Couldn't find how to add comments in ryml docs)

  //Display
  {
    auto node = node_root["Display"];
    node |= ryml::MAP;

    node["VSync"] << RendererCVar::cvar_vsync.get();
  }

  //Color
  {
    auto node = node_root["Color"];
    node |= ryml::MAP;

    node["Tonemapper"] << RendererCVar::cvar_tonemapper.get();
    node["Exposure"] << RendererCVar::cvar_exposure.get();
    node["Gamma"] << RendererCVar::cvar_gamma.get();
  }

  //GTAO
  {
    auto node = node_root["GTAO"];
    node |= ryml::MAP;

    node["Enabled"] << RendererCVar::cvar_gtao_enable.get();
  }

  //Bloom
  {
    auto node = node_root["Bloom"];
    node |= ryml::MAP;

    node["Enabled"] << RendererCVar::cvar_bloom_enable.get();
    node["Threshold"] << RendererCVar::cvar_bloom_threshold.get();
  }

  //SSR
  {
    auto node = node_root["SSR"];
    node |= ryml::MAP;

    node["Enabled"] << RendererCVar::cvar_ssr_enable.get();
  }

  //DirectShadows
  {
    auto node = node_root["DirectShadows"];
    node |= ryml::MAP;

    node["UsePCF"] << RendererCVar::cvar_shadows_pcf.get();
  }

  //FXAA
  {
    auto node = node_root["FXAA"];
    node |= ryml::MAP;

    node["Enabled"] << RendererCVar::cvar_fxaa_enable.get();
  }

  std::stringstream ss;
  ss << tree;
  std::ofstream filestream(path);
  filestream << ss.str();
}

bool RendererConfig::load_config(const char* path) {
  const auto& content = FileUtils::read_file(path);
  if (content.empty())
    return false;

  ryml::Tree tree = ryml::parse_in_arena(c4::to_csubstr(content));
  const ryml::ConstNodeRef root = tree.rootref();
  const ryml::ConstNodeRef nodeRoot = root;

  //Display
  {
    const ryml::ConstNodeRef node = nodeRoot["Display"];
    auto vsync = 1;
    node["VSync"] >> vsync;
    RendererCVar::cvar_vsync.set(vsync);
  }

  //Color
  {
    const ryml::ConstNodeRef node = nodeRoot["Color"];
    int tonemapper;
    float exposure, gamma;
    node["Tonemapper"] >> tonemapper;
    node["Exposure"] >> exposure;
    node["Gamma"] >> gamma;
    RendererCVar::cvar_tonemapper.set(tonemapper);
    RendererCVar::cvar_exposure.set(exposure);
    RendererCVar::cvar_gamma.set(gamma);
  }

  //GTAO
  {
    const ryml::ConstNodeRef node = nodeRoot["GTAO"];
    int enabled;
    node["Enabled"] >> enabled;
    RendererCVar::cvar_gtao_enable.set(enabled);
  }

  //Bloom
  {
    const ryml::ConstNodeRef node = nodeRoot["Bloom"];
    int enabled;
    float threshold;
    node["Enabled"] >> enabled;
    node["Threshold"] >> threshold;
    RendererCVar::cvar_bloom_enable.set(enabled);
    RendererCVar::cvar_bloom_threshold.set(threshold);
  }

  //SSR
  {
    const ryml::ConstNodeRef node = nodeRoot["SSR"];
    int enabled;
    node["Enabled"] >> enabled;
    RendererCVar::cvar_ssr_enable.set(enabled);
  }

  //DirectShadows
  {
    const ryml::ConstNodeRef node = nodeRoot["DirectShadows"];
    int use_pcf;
    node["UsePCF"] >> use_pcf;
    RendererCVar::cvar_shadows_pcf.set(use_pcf);
  }

  //FXAA
  {
    const ryml::ConstNodeRef node = nodeRoot["FXAA"];
    int enabled;
    node["Enabled"] >> enabled;
    RendererCVar::cvar_fxaa_enable.set(enabled);
  }

  return true;
}
}
