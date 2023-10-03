#pragma once

#include <plf_colony.h>
#include <vuk/Image.hpp>
#include <vuk/SampledImage.hpp>

#include "imgui.h"
#include "Core/Base.h"
#include "Core/Layer.h"
#include <vulkan/vulkan.h>

namespace Oxylus {
class Texture;

class ImGuiLayer : public Layer {
public:
  struct ImGuiData {
    vuk::Texture font_texture;
    vuk::SamplerCreateInfo font_sci;
    std::unique_ptr<vuk::SampledImage> font_si;
  };

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

  void on_attach(EventDispatcher& dispatcher) override;
  void on_detach() override;

  void Begin();
  void End();

  vuk::Future RenderDrawData(vuk::Allocator& allocator, vuk::Future target, ImDrawData* drawData) const;

  vuk::SampledImage* AddSampledImage(const vuk::SampledImage& sampledImage);

  ImGuiIO* GetImGuiIO() const { return m_ImGuiIO; }
  static void ApplyTheme(bool dark = true);
  static void SetStyle();

  plf::colony<vuk::SampledImage> m_SampledImages;
private:
  ImGuiData m_ImGuiData;
  ImGuiIO* m_ImGuiIO = nullptr;

  void InitForVulkan();
  void AddIconFont(float fontSize);
  ImGuiData ImGui_ImplVuk_Init(vuk::Allocator& allocator) const;

};
}
