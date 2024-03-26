#pragma once
#include <cstdint>

#include "Core/ESystem.hpp"

#include "Utils/CVars.hpp"

namespace ox {
namespace RendererCVar {
inline AutoCVar_Int cvar_vsync("rr.vsync", "toggle vsync", 1);

inline AutoCVar_Int cvar_shadows_size("rr.shadows_size", "cascaded shadow map size", 2048);
inline AutoCVar_Int cvar_shadows_pcf("rr.shadows_pcf", "use pcf in cascaded shadows", 1);

inline AutoCVar_Int cvar_draw_grid("rr.draw_grid", "draw editor scene grid", 1);
inline AutoCVar_Float cvar_draw_grid_distance("rr.grid_distance", "max grid distance", 20.f);
inline AutoCVar_Int cvar_draw_bounding_boxes("rr.draw_bounding_boxes", "draw mesh bounding boxes", 0);
inline AutoCVar_Int cvar_draw_physics_shapes("rr.draw_physics_shapes", "draw physics shapes", 0);
inline AutoCVar_Int cvar_enable_debug_renderer("rr.debug_renderer", "draw debug shapes", 1);

inline AutoCVar_Int cvar_reload_render_pipeline("rr.reload_render_pipeline", "reload current scene's render pipeline", 0);

inline AutoCVar_Int cvar_ssr_enable("pp.ssr", "use ssr", 1);
inline AutoCVar_Int cvar_ssr_samples("pp.ssr_samples", "ssr samples", 30);
inline AutoCVar_Float cvar_ssr_max_dist("pp.ssr_max_dist", "ssr max distance", 50.0);

inline AutoCVar_Int cvar_gtao_enable("pp.gtao", "use gtao", 1);
inline AutoCVar_Int cvar_gtao_quality_level("pp.gtao_quality_level", "gtao quality level", 1);
inline AutoCVar_Int cvar_gtao_denoise_passes("pp.gtao_denoise_passes", "amount of gtao denoise blur passes", 3);
inline AutoCVar_Float cvar_gtao_radius("pp.gtao_radius", "gtao radius", 0.5f);
inline AutoCVar_Float cvar_gtao_falloff_range("pp.gtao_falloff_range", "gtao falloff range", 0.615f);
inline AutoCVar_Float cvar_gtao_sample_distribution_power("pp.gtao_sample_distribution_power", "gtao sample distribution power", 2.0f);
inline AutoCVar_Float cvar_gtao_thin_occluder_compensation("pp.gtao_thin_occluder_compensation", "gtao thin occluder compensation", 0.0f);
inline AutoCVar_Float cvar_gtao_final_value_power("pp.gtao_final_value_power", "gtao final value power", 0.5f);
inline AutoCVar_Float cvar_gtao_depth_mip_sampling_offset("pp.gtao_depth_mip_sampling_offset", "gtao depth mip sampling offset", 3.30f);

inline AutoCVar_Int cvar_bloom_enable("pp.bloom", "use bloom", 1);
inline AutoCVar_Float cvar_bloom_threshold("pp.bloom_threshold", "bloom threshold", 1);
inline AutoCVar_Float cvar_bloom_clamp("pp.bloom_clamp", "bloom clmap", 3);

inline AutoCVar_Int cvar_fxaa_enable("pp.fxaa", "use fxaa", 1);

inline AutoCVar_Int cvar_fsr_enable("pp.fsr", "use FSR", 1);

inline AutoCVar_Int cvar_tonemapper("pp.tonemapper", "tonemapper preset", 0);
inline AutoCVar_Float cvar_exposure("pp.exposure", "tonemapping exposure", 1.0f);
inline AutoCVar_Float cvar_gamma("pp.gamma", "screen gamma", 2.2f);
} // namespace RendererCVar

class RendererConfig : public ESystem {
public:
  enum Tonemaps {
    TONEMAP_ACES = 0,
    TONEMAP_UNCHARTED,
    TONEMAP_FILMIC,
    TONEMAP_REINHARD,
  };

  RendererConfig() = default;

  void init() override;
  void deinit() override;

  void save_config(const char* path) const;
  bool load_config(const char* path);
};
} // namespace ox
