#include "RendererSettingsPanel.h"

#include <icons/IconsMaterialDesignIcons.h>

#include "imgui.h"
#include "Core/Application.h"
#include "Render/Vulkan/VulkanRenderer.h"
#include "UI/IGUI.h"
#include "Utils/UIUtils.h"

namespace Oxylus {
  RendererSettingsPanel::RendererSettingsPanel() : EditorPanel("Renderer Settings", ICON_MDI_GPU, true) { }

  void RendererSettingsPanel::OnImGuiRender() {
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
      ImGui::Text("FPS: %lf", static_cast<double>(avg));
      const double fps = (1.0 / static_cast<double>(avg)) * 1000.0;
      ImGui::Text("Frame time (ms): %lf", fps);

      ImGui::Text("Environment");
      IGUI::BeginProperties();
      const char* tonemaps[4] = {"ACES", "Uncharted2", "Filmic", "Reinhard"};
      ConfigProperty(IGUI::Property("Tonemapper", RendererConfig::Get()->ColorConfig.Tonemapper, tonemaps, 4));
      ConfigProperty(IGUI::Property<float>("Exposure", RendererConfig::Get()->ColorConfig.Exposure, 0, 5, "%.2f"));
      ConfigProperty(IGUI::Property<float>("Gamma", RendererConfig::Get()->ColorConfig.Gamma, 0, 5, "%.2f"));
      IGUI::EndProperties();

      ImGui::Text("SSAO");
      IGUI::BeginProperties();
      ConfigProperty(IGUI::Property("Enabled", RendererConfig::Get()->SSAOConfig.Enabled));
      ConfigProperty(IGUI::Property<float>("Radius", RendererConfig::Get()->SSAOConfig.Radius, 0, 1));
      IGUI::EndProperties();

      ImGui::Text("Bloom");
      IGUI::BeginProperties();
      ConfigProperty(IGUI::Property("Enabled", RendererConfig::Get()->BloomConfig.Enabled));
      ConfigProperty(IGUI::Property<float>("Threshold", RendererConfig::Get()->BloomConfig.Threshold, 0, 5));
      ConfigProperty(IGUI::Property<float>("Clamp", RendererConfig::Get()->BloomConfig.Clamp, 0, 5));
      IGUI::EndProperties();

      ImGui::Text("SSR");
      IGUI::BeginProperties();
      ConfigProperty(IGUI::Property("Enabled", RendererConfig::Get()->SSRConfig.Enabled));
      ConfigProperty(IGUI::Property<>("Samples", RendererConfig::Get()->SSRConfig.Samples, 30, 1024));
      ConfigProperty(IGUI::Property<>("Max Distance", RendererConfig::Get()->SSRConfig.MaxDist, 50.0f, 500.0f));
      IGUI::EndProperties();

      OnEnd();
    }
  }

  void RendererSettingsPanel::ConfigProperty(bool value) {
    if (value)
      RendererConfig::Get()->ConfigChangeDispatcher.trigger(RendererConfig::ConfigChangeEvent{});
  }
}
