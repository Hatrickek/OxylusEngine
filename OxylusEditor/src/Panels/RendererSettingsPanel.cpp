#include "RendererSettingsPanel.h"

#include <icons/IconsMaterialDesignIcons.h>

#include "imgui.h"
#include "Core/Application.h"

#include "Render/RendererConfig.h"
#include "Render/Vulkan/VulkanContext.h"
#include "UI/OxUI.h"

namespace Oxylus {
RendererSettingsPanel::RendererSettingsPanel() : EditorPanel("Renderer Settings", ICON_MDI_GPU, true) { }

void RendererSettingsPanel::on_imgui_render() {
  if (on_begin()) {
    float avg = 0.0;

    const size_t size = m_FrameTimes.size();
    if (size >= 50)
      m_FrameTimes.erase(m_FrameTimes.begin());

    m_FrameTimes.emplace_back(ImGui::GetIO().Framerate);
    for (uint32_t i = 0; i < size; i++) {
      const float frameTime = m_FrameTimes[i];
      m_FpsValues[i] = frameTime;
      avg += frameTime;
    }
    avg /= (float)size;
    const double fps = (1.0 / static_cast<double>(avg)) * 1000.0;
    ImGui::Text("FPS: %lf / (ms): %lf", static_cast<double>(avg), fps);
    ImGui::Text("GPU: %s", VulkanContext::get()->device_name.c_str());

    ImGui::Text("Environment");
    if (OxUI::begin_properties()) {
      const char* tonemaps[4] = {"ACES", "Uncharted2", "Filmic", "Reinhard"};
      OxUI::property("Tonemapper", RendererCVAR::cvar_tonemapper.get_ptr(), tonemaps, 4);
      OxUI::property<float>("Exposure", RendererCVAR::cvar_exposure.get_ptr(), 0, 5, "%.2f");
      OxUI::property<float>("Gamma", RendererCVAR::cvar_gamma.get_ptr(), 0, 5, "%.2f");
      OxUI::end_properties();
    }
    ImGui::Text("GTAO");
    if (OxUI::begin_properties()) {
      OxUI::property("Enabled", (bool*)RendererCVAR::cvar_gtao_enable.get_ptr());
      OxUI::property<int>("Denoise Passes", RendererCVAR::cvar_gtao_denoise_passes.get_ptr(), 1, 5);
      OxUI::property<float>("Radius", RendererCVAR::cvar_gtao_radius.get_ptr(), 0, 1);
      OxUI::property<float>("Falloff Range", RendererCVAR::cvar_gtao_falloff_range.get_ptr(), 0, 1);
      OxUI::property<float>("Sample Distribution Power", RendererCVAR::cvar_gtao_sample_distribution_power.get_ptr(), 0, 5);
      OxUI::property<float>("Thin Occluder Compensation", RendererCVAR::cvar_gtao_thin_occluder_compensation.get_ptr(), 0, 5);
      OxUI::property<float>("Final Value Power", RendererCVAR::cvar_gtao_final_value_power.get_ptr(), 0, 5);
      OxUI::property<float>("Depth Mip Sampling Offset", RendererCVAR::cvar_gtao_depth_mip_sampling_offset.get_ptr(), 0, 5);
      OxUI::end_properties();
    }
    ImGui::Text("Bloom");
    if (OxUI::begin_properties()) {
      OxUI::property("Enabled", (bool*)RendererCVAR::cvar_bloom_enable.get_ptr());
      OxUI::property<float>("Threshold", RendererCVAR::cvar_bloom_threshold.get_ptr(), 0, 5);
      OxUI::property<float>("Clamp", RendererCVAR::cvar_bloom_clamp.get_ptr(), 0, 5);
      OxUI::end_properties();
    }

    ImGui::Text("SSR");
    if (OxUI::begin_properties()) {
      OxUI::property("Enabled", (bool*)RendererCVAR::cvar_ssr_enable.get_ptr());
      OxUI::property("Samples", RendererCVAR::cvar_ssr_samples.get_ptr(), 30, 1024);
      OxUI::property("Max Distance", RendererCVAR::cvar_ssr_max_dist.get_ptr(), 50.0f, 500.0f);
      OxUI::end_properties();
    }
    ImGui::Text("FXAA");
    if (OxUI::begin_properties()) {
      OxUI::property("Enabled", (bool*)RendererCVAR::cvar_fxaa_enable.get_ptr());
      OxUI::end_properties();
    }
  }
  on_end();
}
}
