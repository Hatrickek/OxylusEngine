#include "RendererSettingsPanel.h"

#include <icons/IconsMaterialDesignIcons.h>

#include "imgui.h"
#include "Core/Application.h"
#include "Render/Vulkan/VulkanContext.h"
#include "Render/Vulkan/VulkanRenderer.h"
#include "UI/IGUI.h"

namespace Oxylus {
  RendererSettingsPanel::RendererSettingsPanel() : EditorPanel("Renderer Settings", ICON_MDI_GPU, true) { }

  void RendererSettingsPanel::on_imgui_render() {
    if (OnBegin()) {
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
      IGUI::begin_properties();
      const char* tonemaps[4] = {"ACES", "Uncharted2", "Filmic", "Reinhard"};
      ConfigProperty(IGUI::property("Tonemapper", RendererConfig::get()->color_config.tonemapper, tonemaps, 4));
      ConfigProperty(IGUI::property<float>("Exposure", RendererConfig::get()->color_config.exposure, 0, 5, "%.2f"));
      ConfigProperty(IGUI::property<float>("Gamma", RendererConfig::get()->color_config.gamma, 0, 5, "%.2f"));
      IGUI::end_properties();

      ImGui::Text("GTAO");
      IGUI::begin_properties();
      ConfigProperty(IGUI::property("Enabled", RendererConfig::get()->gtao_config.enabled));
      ConfigProperty(IGUI::property<int>("Denoise Passes", RendererConfig::get()->gtao_config.settings.denoise_passes, 1, 5));
      ConfigProperty(IGUI::property<float>("Radius", RendererConfig::get()->gtao_config.settings.radius, 0, 1));
      ConfigProperty(IGUI::property<float>("Radius Multiplier", RendererConfig::get()->gtao_config.settings.radius_multiplier, 0, 5));
      ConfigProperty(IGUI::property<float>("Falloff Range", RendererConfig::get()->gtao_config.settings.falloff_range, 0, 1));
      ConfigProperty(IGUI::property<float>("Sample Distribution Power", RendererConfig::get()->gtao_config.settings.sample_distribution_power, 0, 5));
      ConfigProperty(IGUI::property<float>("Thin Occluder Compensation", RendererConfig::get()->gtao_config.settings.thin_occluder_compensation, 0, 5));
      ConfigProperty(IGUI::property<float>("Final Value Power", RendererConfig::get()->gtao_config.settings.final_value_power, 0, 5));
      ConfigProperty(IGUI::property<float>("Depth Mip Sampling Offset", RendererConfig::get()->gtao_config.settings.depth_mip_sampling_offset, 0, 5));
      IGUI::end_properties();

      ImGui::Text("Bloom");
      IGUI::begin_properties();
      ConfigProperty(IGUI::property("Enabled", RendererConfig::get()->bloom_config.enabled));
      ConfigProperty(IGUI::property<float>("Threshold", RendererConfig::get()->bloom_config.threshold, 0, 5));
      ConfigProperty(IGUI::property<float>("Clamp", RendererConfig::get()->bloom_config.clamp, 0, 5));
      IGUI::end_properties();

      ImGui::Text("SSR");
      IGUI::begin_properties();
      ConfigProperty(IGUI::property("Enabled", RendererConfig::get()->ssr_config.enabled));
      ConfigProperty(IGUI::property("Samples", RendererConfig::get()->ssr_config.samples, 30, 1024));
      ConfigProperty(IGUI::property("Max Distance", RendererConfig::get()->ssr_config.max_dist, 50.0f, 500.0f));
      IGUI::end_properties();

      ImGui::Text("FXAA");
      IGUI::begin_properties();
      ConfigProperty(IGUI::property("Enabled", RendererConfig::get()->fxaa_config.enabled));
      IGUI::end_properties();

      OnEnd();
    }
  }

  void RendererSettingsPanel::ConfigProperty(bool value) {
    if (value)
      RendererConfig::get()->dispatch_config_change();
  }
}
