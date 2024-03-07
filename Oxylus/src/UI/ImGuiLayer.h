#pragma once

#include <plf_colony.h>
#include <vuk/Image.hpp>
#include <vuk/SampledImage.hpp>

#include "imgui.h"
#include "Core/Base.h"
#include "Core/Layer.h"

namespace ox {
class Texture;

class ImGuiLayer : public Layer {
public:
  struct ImGuiData {
    vuk::Texture font_texture;
    vuk::SamplerCreateInfo font_sci;
    std::unique_ptr<vuk::SampledImage> font_si;
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

  vuk::Future render_draw_data(vuk::Allocator& allocator, vuk::Future target, ImDrawData* im_draw_data) const;

  vuk::SampledImage* add_sampled_image(const vuk::SampledImage& sampled_image);

  ImGuiIO* get_imgui_io() const { return imgui_io; }
  static void apply_theme(bool dark = true);
  static void set_style();

  plf::colony<vuk::SampledImage> sampled_images;
private:
  ImGuiData imgui_data;
  ImGuiIO* imgui_io = nullptr;

  void init_for_vulkan();
  void add_icon_font(float font_size);
  ImGuiData imgui_impl_vuk_init(vuk::Allocator& allocator) const;

};
}
