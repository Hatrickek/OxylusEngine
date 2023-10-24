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

    node["VSync"] << display_config.vsync;
  }

  //Color
  {
    auto node = node_root["Color"];
    node |= ryml::MAP;

    node["Tonemapper"] << color_config.tonemapper;
    node["Exposure"] << color_config.exposure;
    node["Gamma"] << color_config.gamma;
  }

  //SSAO
  {
    auto node = node_root["SSAO"];
    node |= ryml::MAP;

    node["Enabled"] << gtao_config.enabled;
    // TODO(hatrickek):
    //node["Radius"] << ssao_config.radius;
  }

  //Bloom
  {
    auto node = node_root["Bloom"];
    node |= ryml::MAP;

    node["Enabled"] << bloom_config.enabled;
    node["Threshold"] << bloom_config.threshold;
  }

  //SSR
  {
    auto node = node_root["SSR"];
    node |= ryml::MAP;

    node["Enabled"] << ssr_config.enabled;
  }

  //DirectShadows
  {
    auto node = node_root["DirectShadows"];
    node |= ryml::MAP;

    node["Enabled"] << direct_shadows_config.enabled;
    node["Quality"] << direct_shadows_config.quality;
    node["UsePCF"] << direct_shadows_config.use_pcf;
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
    node["VSync"] >> display_config.vsync;
  }

  //Color
  {
    const ryml::ConstNodeRef node = nodeRoot["Color"];
    node["Tonemapper"] >> color_config.tonemapper;
    node["Exposure"] >> color_config.exposure;
    node["Gamma"] >> color_config.gamma;
  }

  //SSAO
  {
    const ryml::ConstNodeRef node = nodeRoot["SSAO"];

    node["Enabled"] >> gtao_config.enabled;
    //TODO(hatrickek):
    //node["Radius"] >> ssao_config.radius;
  }

  //Bloom
  {
    const ryml::ConstNodeRef node = nodeRoot["Bloom"];

    node["Enabled"] >> bloom_config.enabled;
    node["Threshold"] >> bloom_config.threshold;
  }

  //SSR
  {
    const ryml::ConstNodeRef node = nodeRoot["SSR"];

    node["Enabled"] >> ssr_config.enabled;
  }

  //DirectShadows
  {
    const ryml::ConstNodeRef node = nodeRoot["DirectShadows"];

    node["Enabled"] >> direct_shadows_config.enabled;
    node["Quality"] >> direct_shadows_config.quality;
    node["UsePCF"] >> direct_shadows_config.use_pcf;
  }

  return true;
}

void RendererConfig::dispatch_config_change() {
  config_change_dispatcher.trigger(ConfigChangeEvent{});
}
}
