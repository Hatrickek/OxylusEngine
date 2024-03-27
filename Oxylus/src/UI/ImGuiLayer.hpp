#pragma once

#include <plf_colony.h>
#include <vuk/Future.hpp>
#include <vuk/Image.hpp>
#include <vuk/SampledImage.hpp>

#include <imgui.h>

#include "Assets/Texture.hpp"
#include "Core/Layer.hpp"

namespace ox {
class ImGuiLayer : public Layer {
public:
  struct ImGuiData {
    Shared<Texture> font_texture = nullptr;
  };

  static ImFont* bold_font;
  static ImFont* regular_font;
  static ImFont* small_font;

  inline static ImVec4 header_selected_color;
  inline static ImVec4 header_hovered_color;
  inline static ImVec4 window_bg_color;
  inline static ImVec4 window_bg_alternative_color;
  inline static ImVec4 asset_icon_color;
  inline static ImVec4 text_color;
  inline static ImVec4 text_disabled_color;

  inline static ImVec2 ui_frame_padding;
  inline static ImVec2 popup_item_spacing;

  ImGuiLayer();
  ~ImGuiLayer() override = default;

  void on_attach(EventDispatcher& dispatcher) override;
  void on_detach() override;

  void begin();
  void end();

  [[nodiscard]] vuk::Value<vuk::ImageAttachment> render_draw_data(vuk::Allocator& allocator, vuk::Value<vuk::ImageAttachment> target) const;

  vuk::SampledImage* add_sampled_image(const vuk::SampledImage& sampled_image);

  static void apply_theme(bool dark = true);
  static void set_style();

  plf::colony<vuk::SampledImage> sampled_images;

private:
  ImGuiData imgui_data;
  ImDrawData* draw_data = nullptr;

  void init_for_vulkan();
  void add_icon_font(float font_size);
  ImGuiData imgui_impl_vuk_init(vuk::Allocator& allocator) const;
};
} // namespace ox
