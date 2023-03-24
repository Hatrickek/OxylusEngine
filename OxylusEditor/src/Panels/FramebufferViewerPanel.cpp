#include "FramebufferViewerPanel.h"
#include "Utils/StringUtils.h"

#include <imgui.h>
#include <icons/IconsMaterialDesignIcons.h>
#include <Render/Vulkan/VulkanFramebuffer.h>

namespace Oxylus {
  FramebufferViewerPanel::FramebufferViewerPanel() : EditorPanel("Framebuffer Viewer", ICON_MDI_IMAGE_FRAME) { }

  void FramebufferViewerPanel::OnImGuiRender() {
    if (OnBegin()) {
      constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_ContextMenuInBody |
                                             ImGuiTableFlags_ScrollY;

      ImGuiTreeNodeFlags flags = 0;
      flags |= ImGuiTreeNodeFlags_OpenOnDoubleClick;
      flags |= ImGuiTreeNodeFlags_SpanFullWidth;
      flags |= ImGuiTreeNodeFlags_FramePadding;
      flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

      static VulkanFramebuffer* s_SelectedFramebuffer = nullptr;

      if (ImGui::BeginTable("Framebuffers", 1, tableFlags)) {
        ImGui::TableSetupColumn(" Name", ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_WidthStretch);
        const auto& framebuffers = FrameBufferPool::GetPool();
        for (const auto& framebuffer : framebuffers) {
          if (framebuffer->GetImage().empty())
            continue;
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::TreeNodeEx(framebuffer->GetDescription().DebugName.c_str(),
                            flags,
                            "%s  %s",
                            StringUtils::FromChar8T(ICON_MDI_IMAGE_FRAME),
                            framebuffer->GetDescription().DebugName.c_str());
          if (!ImGui::IsItemToggledOpen() && (ImGui::IsItemClicked(ImGuiMouseButton_Left) ||
                                              ImGui::IsItemClicked(ImGuiMouseButton_Middle) || ImGui::IsItemClicked(
                  ImGuiMouseButton_Right))) {
            s_SelectedFramebuffer = framebuffer;
          }
        }
        if (s_SelectedFramebuffer)
          DrawViewer(s_SelectedFramebuffer->GetImage()[0].GetDescriptorSet());
        ImGui::EndTable();
      }

      OnEnd();
    }
  }

  void FramebufferViewerPanel::DrawViewer(const vk::DescriptorSet imageDescriptorSet) const {
    if (!imageDescriptorSet)
      return;
    if (ImGui::BeginChild("Frame")) {
      ImGui::Image(imageDescriptorSet, ImGui::GetWindowSize());
      ImGui::EndChild();
    }
  }
}
