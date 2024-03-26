#include "RendererConfig.h"

#include <fstream>
#include <sstream>

#include "Thread/TaskScheduler.hpp"
#include "Core/App.hpp"
#include "Core/FileSystem.hpp"

#include "Utils/Profiler.hpp"
#include "Utils/Toml.hpp"

namespace ox {
void RendererConfig::init() {
  auto* task_scheduler = App::get_system<TaskScheduler>();
  task_scheduler->add_task([this]() {
    if (!load_config("renderer_config.toml"))
      save_config("renderer_config.toml");
  });

  task_scheduler->wait_for_all();
}

void RendererConfig::deinit() { save_config("renderer_config.toml"); }

void RendererConfig::save_config(const char* path) const {
  OX_SCOPED_ZONE;

  const auto root = toml::table{
    {
      "display",
      toml::table{
        {"vsync", (bool)RendererCVar::cvar_vsync.get()},
      },
    },
    {
      "debug",
      toml::table{
        {"debug_renderer", (bool)RendererCVar::cvar_enable_debug_renderer.get()},
        {"bounding_boxes", (bool)RendererCVar::cvar_draw_bounding_boxes.get()},
        {"physics_shapes", (bool)RendererCVar::cvar_draw_physics_shapes.get()},
      },
    },
    {"color",
     toml::table{{"tonemapper", RendererCVar::cvar_tonemapper.get()},
                 {"exposure", RendererCVar::cvar_exposure.get()},
                 {"gamma", RendererCVar::cvar_gamma.get()}}},
    {
      "gtao",
      toml::table{
        {"enabled", (bool)RendererCVar::cvar_gtao_enable.get()},
      },
    },
    {
      "bloom",
      toml::table{
        {"enabled", (bool)RendererCVar::cvar_bloom_enable.get()},
        {"threshold", RendererCVar::cvar_bloom_threshold.get()},
      },
    },
    {
      "ssr",
      toml::table{
        {"enabled", (bool)RendererCVar::cvar_ssr_enable.get()},
      },
    },
    {
      "shadows",
      toml::table{
        {"size", RendererCVar::cvar_shadows_size.get()},
      },
    },
    {
      "fxaa",
      toml::table{
        {"enabled", (bool)RendererCVar::cvar_fxaa_enable.get()},
      },
    },
  };

  FileSystem::write_file(path, root, "# Oxylus renderer config file"); // TODO: check result
}

bool RendererConfig::load_config(const char* path) {
  OX_SCOPED_ZONE;
  const auto& content = FileSystem::read_file(path);
  if (content.empty())
    return false;

  toml::table toml = toml::parse(content);

  const auto display_config = toml["display"];
  RendererCVar::cvar_vsync.set(display_config["vsync"].as_boolean()->get());

  const auto debug_config = toml["debug"];
  RendererCVar::cvar_enable_debug_renderer.set(debug_config["debug_renderer"].as_boolean()->get());
  RendererCVar::cvar_draw_bounding_boxes.set(debug_config["bounding_boxes"].as_boolean()->get());
  RendererCVar::cvar_draw_physics_shapes.set(debug_config["physics_shapes"].as_boolean()->get());

  const auto color_config = toml["color"];
  RendererCVar::cvar_tonemapper.set((int)color_config["tonemapper"].as_integer()->get());
  RendererCVar::cvar_exposure.set((float)color_config["exposure"].as_floating_point()->get());
  RendererCVar::cvar_gamma.set((float)color_config["gamma"].as_floating_point()->get());

  const auto gtao_config = toml["gtao"];
  RendererCVar::cvar_gtao_enable.set(gtao_config["enabled"].as_boolean()->get());

  const auto bloom_config = toml["bloom"];
  RendererCVar::cvar_bloom_enable.set(bloom_config["enabled"].as_boolean()->get());
  RendererCVar::cvar_bloom_threshold.set((float)bloom_config["threshold"].as_floating_point()->get());

  const auto ssr_config = toml["ssr"];
  RendererCVar::cvar_ssr_enable.set(ssr_config["enabled"].as_boolean()->get());

  const auto shadows_config = toml["shadows"];
  RendererCVar::cvar_shadows_size.set((int)shadows_config["size"].as_integer()->get());

  const auto fxaa_config = toml["fxaa"];
  RendererCVar::cvar_fxaa_enable.set(fxaa_config["enabled"].as_boolean()->get());

  return true;
}
} // namespace ox
