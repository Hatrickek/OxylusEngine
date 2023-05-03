#pragma once
#include <cstdint>

#include "Event/Event.h"

namespace Oxylus {
  class RendererConfig {
  public:
    struct ConfigChangeEvent { };

    EventDispatcher ConfigChangeDispatcher;

    struct Display {
      bool VSync = true;
    } DisplayConfig;

    enum Tonemaps {
      TONEMAP_ACES = 0,
      TONEMAP_UNCHARTED,
      TONEMAP_FILMIC,
      TONEMAP_REINHARD,
    };

    struct Color {
      int Tonemapper = TONEMAP_ACES;
      float Exposure = 1.0f;
      float Gamma = 2.5f;
    } ColorConfig;

    struct SSAO {
      bool Enabled = false;
      float Radius = 0.2f;
    } SSAOConfig;

    struct Bloom {
      bool Enabled = true;
      float Threshold = 1.0f;
      float Clamp = 3.0f;
    } BloomConfig;

    struct SSR {
      bool Enabled = true;
      int Samples = 30;
      float MaxDist = 50.0f;
    } SSRConfig;

    struct DirectShadows {
      bool Enabled = true;
      uint32_t Quality = 3;
      bool UsePCF = true;
      uint32_t Size = 2048;
    } DirectShadowsConfig;

    RendererConfig();
    ~RendererConfig() = default;

    void SaveConfig(const char* path) const;
    bool LoadConfig(const char* path);

    static RendererConfig* Get() { return s_Instance; }

  private:
    static RendererConfig* s_Instance;
  };
}
