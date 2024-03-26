﻿#include "StatisticsPanel.hpp"

#include <icons/IconsMaterialDesignIcons.h>
#include <imgui.h>

namespace ox {
  StatisticsPanel::StatisticsPanel() : EditorPanel("Statistics", ICON_MDI_CLIPBOARD_TEXT, false) {}

  void StatisticsPanel::on_imgui_render() {
    if (on_begin()) {
      if (ImGui::BeginTabBar("TabBar")) {
        if (ImGui::BeginTabItem("Memory")) {
          MemoryTab();
          ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Renderer")) {
          RendererTab();
          ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
      }
      on_end();
    }
  }

  void StatisticsPanel::MemoryTab() const {
#if 0
    static bool showInMegabytes;
    ImGui::Checkbox("Show in megabytes", &showInMegabytes);
    ImGui::Separator();
    const auto sizetype = showInMegabytes ? "mb" : "kb";
    {
      ImGui::Text("RAM");
      auto totalAllocated = showInMegabytes ? (float)Memory::TotalAllocated / 1024.0f / 1024.0f : (float)Memory::TotalAllocated / 1024.0f;
      auto totalFreed = showInMegabytes ? (float)Memory::TotalFreed / 1024.0f / 1024.0f : (float)Memory::TotalFreed / 1024.0f;
      auto currentUsage = showInMegabytes ? (float)Memory::CurrentUsage() / 1024.0f / 1024.0f : (float)Memory::CurrentUsage() / 1024.0f;
      ImGui::Text("%s", fmt::format("Total Allocated: {0} {1}", totalAllocated, sizetype).c_str());
      ImGui::Text("%s", fmt::format("Total Freed: {0} {1}", totalFreed, sizetype).c_str());
      ImGui::Text("%s", fmt::format("Current Usage: {0} {1}", currentUsage, sizetype).c_str());
    }
    ImGui::Separator();
    //GPU
    {
      ImGui::Text("GPU");
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::BeginTooltip();
        ImGui::Text("Only image and buffer allocations are included.");
        ImGui::EndTooltip();
      }
      auto totalAllocated = showInMegabytes ? (float)GPUMemory::TotalAllocated / 1024.0f / 1024.0f : (float)GPUMemory::TotalAllocated / 1024.0f;
      auto totalFreed = showInMegabytes ? (float)GPUMemory::TotalFreed / 1024.0f / 1024.0f : (float)GPUMemory::TotalFreed / 1024.0f;
      auto currentUsage = showInMegabytes ? (float)GPUMemory::CurrentUsage() / 1024.0f / 1024.0f : (float)GPUMemory::CurrentUsage() / 1024.0f;
      ImGui::Text("%s", fmt::format("Total Allocated: {0} {1}", totalAllocated, sizetype).c_str());
      ImGui::Text("%s", fmt::format("Total Freed: {0} {1}", totalFreed, sizetype).c_str());
      ImGui::Text("%s", fmt::format("Current Usage: {0} {1}", currentUsage, sizetype).c_str());
    }
#endif
  }

  void StatisticsPanel::RendererTab() {
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
  }
}
