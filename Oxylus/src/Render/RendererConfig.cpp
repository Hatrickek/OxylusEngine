#include "RendererConfig.h"

#include "Core/YamlHelpers.h"
#include "Utils/FileUtils.h"

#include <fstream>
#include <sstream>
#include <toml++/toml.hpp>

#include "Utils/Profiler.h"

namespace Oxylus {
RendererConfig* RendererConfig::s_instance = nullptr;

RendererConfig::RendererConfig() {
  s_instance = this;
}

void RendererConfig::save_config(const char* path) const {
  OX_SCOPED_ZONE;

  const auto root = toml::table{
    {
      "display",
      toml::table{
        {"vsync", (bool)RendererCVar::cvar_vsync.get()},
      }
    },
    {
      "color",
      toml::table{
        {"tonemapper", RendererCVar::cvar_tonemapper.get()},
        {"exposure", RendererCVar::cvar_exposure.get()},
        {"gamma", RendererCVar::cvar_gamma.get()}
      }
    },
    {
      "gtao",
      toml::table{
        {"enabled", (bool)RendererCVar::cvar_gtao_enable.get()},
      }
    },
    {
      "bloom",
      toml::table{
        {"enabled", (bool)RendererCVar::cvar_bloom_enable.get()},
        {"threshold", RendererCVar::cvar_bloom_threshold.get()},
      }
    },
    {
      "ssr",
      toml::table{
        {"enabled", (bool)RendererCVar::cvar_ssr_enable.get()},
      }
    },
    {
      "shadows",
      toml::table{
        {"size", RendererCVar::cvar_shadows_size.get()},
      }
    },
    {
      "fxaa",
      toml::table{
        {"enabled", (bool)RendererCVar::cvar_fxaa_enable.get()},
      }
    }
  };

  std::stringstream ss;
  ss << root;
  std::ofstream filestream(path);
  filestream << ss.str();
}

bool RendererConfig::load_config(const char* path) {
  OX_SCOPED_ZONE;
  const auto& content = FileUtils::read_file(path);
  if (content.empty())
    return false;

  toml::table toml = toml::parse(content);

  const auto config = toml["renderer_config"];

  const auto display_config = config["display"];
  RendererCVar::cvar_vsync.set(display_config["vsync"].as_boolean()->get());

  const auto color_config = config["color"];
  RendererCVar::cvar_tonemapper.set((int)color_config["tonemapper"].as_integer()->get());
  RendererCVar::cvar_exposure.set((float)color_config["exposure"].as_floating_point()->get());
  RendererCVar::cvar_gamma.set((float)color_config["gamma"].as_floating_point()->get());

  const auto gtao_config = config["gtao"];
  RendererCVar::cvar_gtao_enable.set(gtao_config["enabled"].as_boolean()->get());

  const auto bloom_config = config["bloom"];
  RendererCVar::cvar_bloom_enable.set(bloom_config["enabled"].as_boolean()->get());
  RendererCVar::cvar_bloom_threshold.set((float)bloom_config["threshold"].as_floating_point()->get());

  const auto ssr_config = config["ssr"];
  RendererCVar::cvar_ssr_enable.set(ssr_config["enabled"].as_boolean()->get());

  const auto shadows_config = config["shadows"];
  RendererCVar::cvar_shadows_size.set((int)shadows_config["size"].as_integer()->get());

  const auto fxaa_config = config["fxaa"];
  RendererCVar::cvar_fxaa_enable.set(fxaa_config["enabled"].as_boolean()->get());

  return true;
}
}
