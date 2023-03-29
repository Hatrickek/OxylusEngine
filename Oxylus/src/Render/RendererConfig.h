#pragma once
#include <cstdint>

namespace Oxylus {
  class RendererConfig {
  public:
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

    struct Vignette {
      bool Enabled = true;
      float Intensity = 0.25f;
    } VignetteConfig;

    struct SSAO {
      bool Enabled = false;
      float Radius = 0.2f;
    } SSAOConfig;

    struct Bloom {
      bool Enabled = true;
      float Threshold = 1.0f;
      float Knee = 0.1f;
      float Clamp = 100.0f;
    } BloomConfig;

    struct SSR {
      bool Enabled = true;
    } SSRConfig;

    struct DirectShadows {
      bool Enabled = true;
      uint32_t Quality = 3;
      bool UsePCF = true;
      uint32_t Size = 4096;
    } DirectShadowsConfig;

    RendererConfig();
    ~RendererConfig() = default;

    void SaveConfig(const char* path) const;
    bool LoadConfig(const char* path);

    static RendererConfig* Get() {
      return s_Instance;
    }

  private:
    static RendererConfig* s_Instance;
  };
}
