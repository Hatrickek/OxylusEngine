#pragma once

#include <imgui.h>
#include "Core/layer.h"
#include "Render/Vulkan/VulkanImage.h"

namespace Oxylus {
  class ImGuiLayer : public Layer {
  public:
    int SelectedTheme = 0;
    static ImFont* BoldFont;
    static ImFont* RegularFont;
    static ImFont* SmallFont;

    inline static ImVec4 HeaderSelectedColor;
    inline static ImVec4 HeaderHoveredColor;
    inline static ImVec4 WindowBgColor;
    inline static ImVec4 WindowBgAlternativeColor;
    inline static ImVec4 AssetIconColor;
    inline static ImVec4 TextColor;
    inline static ImVec4 TextDisabledColor;

    inline static ImVec2 UIFramePadding;
    inline static ImVec2 PopupItemSpacing;

    ImGuiLayer();
    ~ImGuiLayer() override = default;

    void OnAttach(EventDispatcher& dispatcher) override;
    void OnDetach() override;

    static void Begin();
    void RenderDrawData(const vk::CommandBuffer& commandBuffer, const vk::Pipeline& pipeline) const;

    ImGuiIO* GetImGuiIO() const { return m_ImGuiIO; }

    void End();
    void SetTheme(int index);

  private:
    ImDrawData* m_DrawData = nullptr;
    ImGuiIO* m_ImGuiIO = nullptr;
    VulkanImage m_FontImage;

    void InitForVulkan();
    void AddIconFont(float fontSize);

    void ImGuiDarkTheme();
  };
}
