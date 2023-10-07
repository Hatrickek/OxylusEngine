#pragma once
#include <cstdint>

#include "Event/Event.h"

namespace Oxylus {
class RendererConfig {
public:
  struct ConfigChangeEvent { };

  EventDispatcher config_change_dispatcher;

  struct Display {
    bool vsync = true;
  } display_config;

  enum Tonemaps {
    TONEMAP_ACES = 0,
    TONEMAP_UNCHARTED,
    TONEMAP_FILMIC,
    TONEMAP_REINHARD,
  };

  struct Color {
    int tonemapper = TONEMAP_ACES;
    float exposure = 1.0f;
    float gamma = 2.5f;
  } color_config;

  struct SSAO {
    bool enabled = false;
    float radius = 0.2f;
  } ssao_config;

  struct Bloom {
    bool enabled = true;
    float threshold = 1.0f;
    float clamp = 3.0f;
  } bloom_config;

  struct SSR {
    bool enabled = true;
    int samples = 30;
    float max_dist = 50.0f;
  } ssr_config;

  struct DirectShadows {
    bool enabled = true;
    uint32_t quality = 3;
    bool use_pcf = true;
    uint32_t size = 4096;
  } direct_shadows_config;

  struct FXAA {
    bool enabled = true;
  } fxaa_config;

  RendererConfig();
  ~RendererConfig() = default;

  void save_config(const char* path) const;
  bool load_config(const char* path);

  void dispatch_config_change();

  static RendererConfig* get() { return s_instance; }

private:
  static RendererConfig* s_instance;
};
}
