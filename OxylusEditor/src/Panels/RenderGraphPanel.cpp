#include "RenderGraphPanel.h"

#include <RenderGraphUtil.hpp>

#include <icons/IconsMaterialDesignIcons.h>

#include <vuk/RenderGraph.hpp>
#include <vuk/RenderGraphReflection.hpp>
#include <vuk/SampledImage.hpp>
#include <vuk/Partials.hpp>

#include "imgui.h"

#include "Render/RenderPipeline.h"
#include "Render/Utils/VukCommon.h"

#include "Scene/Scene.h"
#include "Scene/SceneRenderer.h"

#include "UI/OxUI.h"

#include "Utils/StringUtils.h"

namespace ox {
RenderGraphPanel::RenderGraphPanel() : EditorPanel("RenderGraph", ICON_MDI_CHART_BAR_STACKED, false) {}

void RenderGraphPanel::on_imgui_render() {
  if (!context)
    return;

  const auto rp = context->get_renderer()->get_render_pipeline();
  const auto rg = rp->get_frame_render_graph();
  const auto compiler = rp->get_compiler();
  const auto chain_links = compiler->get_use_chains();

  if (on_begin()) {
    const float filter_cursor_pos_x = ImGui::GetCursorPosX();
    ImGuiTextFilter name_filter;

    name_filter.Draw("##rg_filter", ImGui::GetContentRegionAvail().x - (OxUI::get_icon_button_size(ICON_MDI_PLUS, "").x + 2.0f * ImGui::GetStyle().FramePadding.x));

    if (!name_filter.IsActive()) {
      ImGui::SameLine();
      ImGui::SetCursorPosX(filter_cursor_pos_x + ImGui::GetFontSize() * 0.5f);
      ImGui::TextUnformatted(StringUtils::from_char8_t(ICON_MDI_MAGNIFY " Search..."));
    }

    for (const auto& head : chain_links) {
      constexpr ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding;
      const auto maybe_name = compiler->get_last_use_name(head);
      if (maybe_name && (name_filter.PassFilter(maybe_name->name.c_str()) 
          && ImGui::TreeNodeEx(maybe_name->name.c_str(), flags, "%s", maybe_name->name.c_str()))) {
        const auto attch = compiler->get_chain_attachment(head).attachment;
        auto extent = attch.extent.extent;

        if (attch.extent.sizing == vuk::Sizing::eRelative)
          extent = rp->get_dimension().extent;

        constexpr auto max_height = 300.0f;
        const auto aspect = (float)extent.width / (float)extent.height;
        const auto fixed_width = max_height * aspect;
        ImVec2 image_size = {fixed_width, max_height};

        auto si = vuk::make_sampled_image({rg.get(), vuk::QualifiedName{{}, maybe_name->name}}, vuk::LinearSamplerClamped);
        OxUI::image(si, image_size);

        ImGui::TreePop();
      }
    }

    on_end();
  }
}
}
