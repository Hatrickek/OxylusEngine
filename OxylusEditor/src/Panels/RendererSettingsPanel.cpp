#include "RendererSettingsPanel.hpp"

#include <icons/IconsMaterialDesignIcons.h>

#include "imgui.h"

#include "Render/Renderer.hpp"
#include "Render/Vulkan/VkContext.hpp"
#include "UI/OxUI.hpp"
#include "Render/RendererConfig.h"

namespace ox {
RendererSettingsPanel::RendererSettingsPanel() : EditorPanel("Renderer Settings", ICON_MDI_GPU, true) {}

void RendererSettingsPanel::on_imgui_render() {
  if (on_begin()) {
    float avg = 0.0;

    const size_t size = m_FrameTimes.size();
    if (size >= 50)
      m_FrameTimes.erase(m_FrameTimes.begin());

    m_FrameTimes.emplace_back(ImGui::GetIO().Framerate);
    for (uint32_t i = 0; i < size; i++) {
      const float frame_time = m_FrameTimes[i];
      m_FpsValues[i] = frame_time;
      avg += frame_time;
    }
    avg /= (float)size;
    const double fps = 1.0 / static_cast<double>(avg) * 1000.0;
    ImGui::Text("FPS: %lf / (ms): %lf", static_cast<double>(avg), fps);
    ImGui::Text("GPU: %s", VkContext::get()->device_name.c_str());
    ImGui::Text("Internal Render Size: [ %u, %u ]", Renderer::get_viewport_width(), Renderer::get_viewport_height());
    OxUI::tooltip("Current viewport resolution");

    ImGui::Separator();
    if (OxUI::icon_button(ICON_MDI_RELOAD, "Reload render pipeline"))
      RendererCVar::cvar_reload_render_pipeline.toggle();
    ImGui::SeparatorText("Debug");
    if (OxUI::begin_properties(OxUI::default_properties_flags, true, 0.3f)) {
      OxUI::property("Draw AABBs", (bool*)RendererCVar::cvar_draw_bounding_boxes.get_ptr());
      OxUI::property("Draw physics shapes", (bool*)RendererCVar::cvar_draw_physics_shapes.get_ptr());
      OxUI::end_properties();
    }

    ImGui::SeparatorText("Environment");
    if (OxUI::begin_properties(OxUI::default_properties_flags, true, 0.3f)) {
      const char* tonemaps[4] = {"ACES", "Uncharted2", "Filmic", "Reinhard"};
      OxUI::property("Tonemapper", RendererCVar::cvar_tonemapper.get_ptr(), tonemaps, 4);
      OxUI::property<float>("Exposure", RendererCVar::cvar_exposure.get_ptr(), 0, 5, "%.2f");
      OxUI::property<float>("Gamma", RendererCVar::cvar_gamma.get_ptr(), 0, 5, "%.2f");
      OxUI::end_properties();
    }

    ImGui::SeparatorText("GTAO");
    if (OxUI::begin_properties(OxUI::default_properties_flags, true, 0.3f)) {
      OxUI::property("Enabled", (bool*)RendererCVar::cvar_gtao_enable.get_ptr());
      OxUI::property<int>("Denoise Passes", RendererCVar::cvar_gtao_denoise_passes.get_ptr(), 1, 5);
      OxUI::property<float>("Radius", RendererCVar::cvar_gtao_radius.get_ptr(), 0, 1);
      OxUI::property<float>("Falloff Range", RendererCVar::cvar_gtao_falloff_range.get_ptr(), 0, 1);
      OxUI::property<float>("Sample Distribution Power", RendererCVar::cvar_gtao_sample_distribution_power.get_ptr(), 0, 5);
      OxUI::property<float>("Thin Occluder Compensation", RendererCVar::cvar_gtao_thin_occluder_compensation.get_ptr(), 0, 5);
      OxUI::property<float>("Final Value Power", RendererCVar::cvar_gtao_final_value_power.get_ptr(), 0, 5);
      OxUI::property<float>("Depth Mip Sampling Offset", RendererCVar::cvar_gtao_depth_mip_sampling_offset.get_ptr(), 0, 5);
      OxUI::end_properties();
    }

    ImGui::SeparatorText("Bloom");
    if (OxUI::begin_properties(OxUI::default_properties_flags, true, 0.3f)) {
      OxUI::property("Enabled", (bool*)RendererCVar::cvar_bloom_enable.get_ptr());
      OxUI::property<float>("Threshold", RendererCVar::cvar_bloom_threshold.get_ptr(), 0, 5);
      OxUI::property<float>("Clamp", RendererCVar::cvar_bloom_clamp.get_ptr(), 0, 5);
      OxUI::end_properties();
    }

    ImGui::SeparatorText("SSR");
    if (OxUI::begin_properties(OxUI::default_properties_flags, true, 0.3f)) {
      OxUI::property("Enabled", (bool*)RendererCVar::cvar_ssr_enable.get_ptr());
      OxUI::property("Samples", RendererCVar::cvar_ssr_samples.get_ptr(), 30, 1024);
      OxUI::property("Max Distance", RendererCVar::cvar_ssr_max_dist.get_ptr(), 50.0f, 500.0f);
      OxUI::end_properties();
    }

    ImGui::SeparatorText("FXAA");
    if (OxUI::begin_properties(OxUI::default_properties_flags, true, 0.3f)) {
      OxUI::property("Enabled", (bool*)RendererCVar::cvar_fxaa_enable.get_ptr());
      OxUI::end_properties();
    }
  }
  on_end();
}
} // namespace ox
