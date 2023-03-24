#pragma once

#include <imgui.h>
#include "Core/layer.h"
#include "Render/Vulkan/VulkanImage.h"

namespace Oxylus {
  class ImGuiLayer : public Layer {
  public:
    ImGuiLayer();
    ~ImGuiLayer() override = default;

    void OnAttach(EventDispatcher& dispatcher) override;
    void OnDetach() override;

    static void Begin();
    void RenderDrawData(const vk::CommandBuffer& commandBuffer, const vk::Pipeline& pipeline) const;

    ImGuiIO* GetImGuiIO() const {
      return m_ImGuiIO;
    }

    void End();

    void SetTheme(int index);

    int SelectedTheme = 0;
    VulkanImage m_FontImage;

  private:
    ImDrawData* m_DrawData = nullptr;
    ImGuiIO* m_ImGuiIO = nullptr;

    void InitForVulkan();
    void ImGuiDarkTheme();
    void ImGuiDarkTheme2();
    void AddIconFont(float fontSize);
  };
}
