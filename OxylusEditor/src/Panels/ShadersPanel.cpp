#include "ShadersPanel.h"
#include <icons/IconsMaterialDesignIcons.h>

#include "imgui.h"
#include "Render/Vulkan/VulkanRenderer.h"
#include "UI/IGUI.h"
#include "Utils/StringUtils.h"

namespace Oxylus {
  ShadersPanel::ShadersPanel() : EditorPanel("Shaders", ICON_MDI_FILE_CHART, false) { }

  void ShadersPanel::OnImGuiRender() {
    if (OnBegin()) {
#if 0
      constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_ContextMenuInBody |
                                             ImGuiTableFlags_ScrollY;

      ImGuiTreeNodeFlags flags = 0;
      flags |= ImGuiTreeNodeFlags_OpenOnArrow;
      flags |= ImGuiTreeNodeFlags_SpanFullWidth;
      flags |= ImGuiTreeNodeFlags_FramePadding;
      flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

      if (ImGui::Button("Reload all")) {
        const auto& shaders = ShaderLibrary::GetShaders();
        for (const auto& [name, shader] : shaders) {
          VulkanRenderer::WaitDeviceIdle();
          shader->Reload();
          shader->Unload();
        }
      }

      if (ImGui::BeginTable("ScrollRegionTable", 2, tableFlags)) {
        ImGui::TableSetupColumn(" Name", ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(" Reload", ImGuiTableColumnFlags_WidthStretch);
        const auto& shaders = ShaderLibrary::GetShaders();
        for (const auto& [name, shader] : shaders) {
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::PushID(shader->GetName().c_str());
          ImGui::TreeNodeEx(shader->GetName().c_str(),
                            flags,
                            "%s  %s",
                            StringUtils::FromChar8T(ICON_MDI_FILE_CHART),
                            shader->GetName().c_str());
          ImGui::TableNextColumn();
          //Align it to far right
          ImGui::SetCursorPosX(
                  ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::CalcTextSize("Reload").x -
                  ImGui::GetScrollX() - 2
                                        * ImGui::GetStyle().ItemSpacing.x);
          if (ImGui::Button("Reload")) {
            VulkanRenderer::WaitDeviceIdle();
            shader->Reload();
            shader->Unload();
          }
          ImGui::PopID();
        }
        ImGui::EndTable();
      }
#endif
      OnEnd();
    }
  }
}
