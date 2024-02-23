#include "ImGuiLayer.h"
#include <imgui.h>
#include <plf_colony.h>
#include <backends/imgui_impl_glfw.h>

#include <glm/common.hpp>

#include <icons/IconsMaterialDesignIcons.h>
#include <icons/MaterialDesign.inl>
#include <vuk/Pipeline.hpp>
#include <vuk/RenderGraph.hpp>
#include <vuk/Partials.hpp>

#include "imgui_frag.h"
#include "imgui_vert.h"
#include "Core/App.h"

#include "GLFW/glfw3.h"

#include "Render/Window.h"
#include "Render/Vulkan/VulkanContext.h"
#include "Utils/Profiler.h"

namespace Ox {
static ImVec4 darken(ImVec4 c, float p) { return {glm::max(0.f, c.x - 1.0f * p), glm::max(0.f, c.y - 1.0f * p), glm::max(0.f, c.z - 1.0f * p), c.w}; }
static ImVec4 lighten(ImVec4 c, float p) { return {glm::max(0.f, c.x + 1.0f * p), glm::max(0.f, c.y + 1.0f * p), glm::max(0.f, c.z + 1.0f * p), c.w}; }

ImFont* ImGuiLayer::regular_font = nullptr;
ImFont* ImGuiLayer::small_font = nullptr;
ImFont* ImGuiLayer::bold_font = nullptr;

ImGuiLayer::ImGuiLayer() : Layer("ImGuiLayer") { }

void ImGuiLayer::on_attach(EventDispatcher&) {
  OX_SCOPED_ZONE;
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard /*| ImGuiConfigFlags_ViewportsEnable*/ |
    ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_DpiEnableScaleFonts |
    ImGuiConfigFlags_DpiEnableScaleViewports;
  io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
  /*io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;*/

  init_for_vulkan();

  apply_theme();
  set_style();
}

void ImGuiLayer::add_icon_font(float font_size) {
  OX_SCOPED_ZONE;
  const ImGuiIO& io = ImGui::GetIO();
  static constexpr ImWchar ICONS_RANGES[] = {ICON_MIN_MDI, ICON_MAX_MDI, 0};
  ImFontConfig icons_config;
  // merge in icons from Font Awesome
  icons_config.MergeMode = true;
  icons_config.PixelSnapH = true;
  icons_config.GlyphOffset.y = 1.0f;
  icons_config.OversampleH = icons_config.OversampleV = 3;
  icons_config.GlyphMinAdvanceX = 4.0f;
  icons_config.SizePixels = font_size;

  io.Fonts->AddFontFromMemoryCompressedTTF(MaterialDesign_compressed_data,
                                           MaterialDesign_compressed_size,
                                           font_size,
                                           &icons_config,
                                           ICONS_RANGES);
}

ImGuiLayer::ImGuiData ImGuiLayer::imgui_impl_vuk_init(vuk::Allocator& allocator) const {
  OX_SCOPED_ZONE;
  vuk::Context& ctx = allocator.get_context();
  auto& io = ImGui::GetIO();
  io.BackendRendererName = "oxylus";
  io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset; // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.

  unsigned char* pixels;
  int width, height;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

  ImGuiData data;
  auto [tex, stub] = create_texture(allocator, vuk::Format::eR8G8B8A8Srgb, vuk::Extent3D{(unsigned)width, (unsigned)height, 1u}, pixels, false);
  data.font_texture = std::move(tex);
  vuk::Compiler comp;
  stub.wait(allocator, comp);
  ctx.set_name(data.font_texture, "ImGui/font");
  vuk::SamplerCreateInfo sci;
  sci.minFilter = sci.magFilter = vuk::Filter::eLinear;
  sci.mipmapMode = vuk::SamplerMipmapMode::eLinear;
  sci.addressModeU = sci.addressModeV = sci.addressModeW = vuk::SamplerAddressMode::eRepeat;
  data.font_sci = sci;
  data.font_si = std::make_unique<vuk::SampledImage>(vuk::SampledImage::Global{*data.font_texture.view, sci, vuk::ImageLayout::eReadOnlyOptimalKHR});
  io.Fonts->TexID = (ImTextureID)data.font_si.get();
  {
    vuk::PipelineBaseCreateInfo pci;
    pci.add_static_spirv(imgui_vert, sizeof(imgui_vert) / 4, "imgui.vert");
    pci.add_static_spirv(imgui_frag, sizeof(imgui_frag) / 4, "imgui.frag");
    ctx.create_named_pipeline("imgui", pci);
  }
  return data;
}


void ImGuiLayer::init_for_vulkan() {
  OX_SCOPED_ZONE;
  ImGui_ImplGlfw_InitForVulkan(Window::get_glfw_window(), true);

  // Upload Fonts
  const auto regular_font_path = App::get_asset_directory("Fonts/jetbrains-mono/JetBrainsMono-Regular.ttf");
  const auto bold_font_path = App::get_asset_directory("Fonts/jetbrains-mono/JetBrainsMono-Bold.ttf");

  const ImGuiIO& io = ImGui::GetIO();
  constexpr float font_size = 16.0f;
  constexpr float font_size_small = 12.0f;

  ImFontConfig fonts_config;
  fonts_config.MergeMode = false;
  fonts_config.PixelSnapH = true;
  fonts_config.OversampleH = fonts_config.OversampleV = 3;
  fonts_config.GlyphMinAdvanceX = 4.0f;
  fonts_config.SizePixels = font_size;

  {
    OX_SCOPED_ZONE_N("Font Loading/Building");
    regular_font = io.Fonts->AddFontFromFileTTF(regular_font_path.c_str(), font_size, &fonts_config);
    add_icon_font(font_size);
    small_font = io.Fonts->AddFontFromFileTTF(regular_font_path.c_str(), font_size_small, &fonts_config);
    add_icon_font(font_size);
    bold_font = io.Fonts->AddFontFromFileTTF(bold_font_path.c_str(), font_size, &fonts_config);
    add_icon_font(font_size);
    io.Fonts->TexGlyphPadding = 1;
    for (int n = 0; n < io.Fonts->ConfigData.Size; n++) {
      ImFontConfig* font_config = &io.Fonts->ConfigData[n];
      font_config->RasterizerMultiply = 1.0f;
    }
    io.Fonts->Build();
  }

  imgui_data = imgui_impl_vuk_init(*VulkanContext::get()->superframe_allocator);
}

void ImGuiLayer::on_detach() {
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}

void ImGuiLayer::begin() {
  OX_SCOPED_ZONE;
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  sampled_images.clear();
}

vuk::Future ImGuiLayer::render_draw_data(vuk::Allocator& allocator, vuk::Future target, ImDrawData* im_draw_data) const {
  OX_SCOPED_ZONE;
  auto reset_render_state = [](const ImGuiData& data, vuk::CommandBuffer& command_buffer, const ImDrawData* draw_data, const vuk::Buffer& vertex, const vuk::Buffer& index) {
    command_buffer.bind_image(0, 0, *data.font_texture.view).bind_sampler(0, 0, data.font_sci);
    if (index.size > 0) {
      command_buffer.bind_index_buffer(index, sizeof(ImDrawIdx) == 2 ? vuk::IndexType::eUint16 : vuk::IndexType::eUint32);
    }
    command_buffer.bind_vertex_buffer(0, vertex, 0, vuk::Packed{vuk::Format::eR32G32Sfloat, vuk::Format::eR32G32Sfloat, vuk::Format::eR8G8B8A8Unorm});
    command_buffer.bind_graphics_pipeline("imgui");
    command_buffer.set_viewport(0, vuk::Rect2D::framebuffer());
    struct PC {
      float scale[2];
      float translate[2];
    } pc;
    pc.scale[0] = 2.0f / draw_data->DisplaySize.x;
    pc.scale[1] = 2.0f / draw_data->DisplaySize.y;
    pc.translate[0] = -1.0f - draw_data->DisplayPos.x * pc.scale[0];
    pc.translate[1] = -1.0f - draw_data->DisplayPos.y * pc.scale[1];
    command_buffer.push_constants(vuk::ShaderStageFlagBits::eVertex, 0, pc);
  };

  const size_t vertex_size = im_draw_data->TotalVtxCount * sizeof(ImDrawVert);
  const size_t index_size = im_draw_data->TotalIdxCount * sizeof(ImDrawIdx);
  auto imvert = *allocate_buffer(allocator, {vuk::MemoryUsage::eCPUtoGPU, vertex_size, 1});
  auto imind = *allocate_buffer(allocator, {vuk::MemoryUsage::eCPUtoGPU, index_size, 1});

  size_t vtx_dst = 0, idx_dst = 0;
  vuk::Compiler comp;
  for (int n = 0; n < im_draw_data->CmdListsCount; n++) {
    const ImDrawList* cmd_list = im_draw_data->CmdLists[n];
    const auto imverto = imvert->add_offset(vtx_dst * sizeof(ImDrawVert));
    const auto imindo = imind->add_offset(idx_dst * sizeof(ImDrawIdx));

    // TODO:
    vuk::host_data_to_buffer(allocator, vuk::DomainFlagBits{}, imverto, std::span(cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size)).wait(allocator, comp);
    vuk::host_data_to_buffer(allocator, vuk::DomainFlagBits{}, imindo, std::span(cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size)).wait(allocator, comp);
    vtx_dst += cmd_list->VtxBuffer.Size;
    idx_dst += cmd_list->IdxBuffer.Size;
  }
  std::shared_ptr<vuk::RenderGraph> rg = std::make_shared<vuk::RenderGraph>("imgui");
  rg->attach_in("target", std::move(target));
  // add rendergraph dependencies to be transitioned
  // make all rendergraph sampled images available
  std::vector<vuk::Resource> resources;
  resources.emplace_back("target", vuk::Resource::Type::eImage, vuk::eColorRW, "target+");
  for (auto& si : sampled_images) {
    if (!si.is_global) {
      resources.emplace_back(si.rg_attachment.reference.rg, si.rg_attachment.reference.name, vuk::Resource::Type::eImage, vuk::Access::eFragmentSampled);
    }
  }
  vuk::Pass pass{
    .name = "imgui",
    .resources = std::move(resources),
    .execute = [this, &allocator, verts = imvert.get(), inds = imind.get(), reset_render_state, im_draw_data](vuk::CommandBuffer& command_buffer) {
      command_buffer.set_dynamic_state(vuk::DynamicStateFlagBits::eViewport | vuk::DynamicStateFlagBits::eScissor);
      command_buffer.set_rasterization(vuk::PipelineRasterizationStateCreateInfo{});
      command_buffer.set_color_blend("target", vuk::BlendPreset::eAlphaBlend);
      reset_render_state(imgui_data, command_buffer, im_draw_data, verts, inds);
      // Will project scissor/clipping rectangles into framebuffer space
      const ImVec2 clip_off = im_draw_data->DisplayPos;         // (0,0) unless using multi-viewports
      const ImVec2 clip_scale = im_draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

      // Render command lists
      // (Because we merged all buffers into a single one, we maintain our own offset into them)
      int global_vtx_offset = 0;
      int global_idx_offset = 0;
      for (int n = 0; n < im_draw_data->CmdListsCount; n++) {
        const ImDrawList* cmd_list = im_draw_data->CmdLists[n];
        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
          const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
          if (pcmd->UserCallback != nullptr) {
            // User callback, registered via ImDrawList::AddCallback()
            // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
            if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
              reset_render_state(imgui_data, command_buffer, im_draw_data, verts, inds);
            else
              pcmd->UserCallback(cmd_list, pcmd);
          }
          else {
            // Project scissor/clipping rectangles into framebuffer space
            ImVec4 clip_rect;
            clip_rect.x = (pcmd->ClipRect.x - clip_off.x) * clip_scale.x;
            clip_rect.y = (pcmd->ClipRect.y - clip_off.y) * clip_scale.y;
            clip_rect.z = (pcmd->ClipRect.z - clip_off.x) * clip_scale.x;
            clip_rect.w = (pcmd->ClipRect.w - clip_off.y) * clip_scale.y;

            const auto fb_width = command_buffer.get_ongoing_render_pass().extent.width;
            const auto fb_height = command_buffer.get_ongoing_render_pass().extent.height;
            if (clip_rect.x < fb_width && clip_rect.y < fb_height && clip_rect.z >= 0.0f && clip_rect.w >= 0.0f) {
              // Negative offsets are illegal for vkCmdSetScissor
              if (clip_rect.x < 0.0f)
                clip_rect.x = 0.0f;
              if (clip_rect.y < 0.0f)
                clip_rect.y = 0.0f;

              // Apply scissor/clipping rectangle
              vuk::Rect2D scissor;
              scissor.offset.x = (int32_t)(clip_rect.x);
              scissor.offset.y = (int32_t)(clip_rect.y);
              scissor.extent.width = (uint32_t)(clip_rect.z - clip_rect.x);
              scissor.extent.height = (uint32_t)(clip_rect.w - clip_rect.y);
              command_buffer.set_scissor(0, scissor);

              // Bind texture
              if (pcmd->TextureId) {
                auto& si = *reinterpret_cast<vuk::SampledImage*>(pcmd->TextureId);
                if (si.is_global) {
                  command_buffer.bind_image(0, 0, si.global.iv).bind_sampler(0, 0, si.global.sci);
                }
                else {
                  if (si.rg_attachment.ivci) {
                    auto ivci = *si.rg_attachment.ivci;
                    const auto res_img = command_buffer.get_resource_image_attachment(si.rg_attachment.reference)->image;
                    ivci.image = res_img.image;
                    auto iv = vuk::allocate_image_view(allocator, ivci);
                    command_buffer.bind_image(0, 0, **iv).bind_sampler(0, 0, si.rg_attachment.sci);
                  }
                  else {
                    command_buffer
                     .bind_image(0,
                                 0,
                                 *command_buffer.get_resource_image_attachment(si.rg_attachment.reference),
                                 vuk::ImageLayout::eShaderReadOnlyOptimal)
                     .bind_sampler(0, 0, si.rg_attachment.sci);
                  }
                }
              }

              // Draw
              command_buffer.draw_indexed(pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0);
            }
          }
        }
        global_idx_offset += cmd_list->IdxBuffer.Size;
        global_vtx_offset += cmd_list->VtxBuffer.Size;
      }
    }
  };

  rg->add_pass(std::move(pass));

  return {std::move(rg), "target+"};
}

vuk::SampledImage* ImGuiLayer::add_sampled_image(const vuk::SampledImage& sampled_image) {
  return &*sampled_images.emplace(sampled_image);
}

void ImGuiLayer::end() {
  OX_SCOPED_ZONE;
  ImGui::Render();

  const ImGuiIO& io = ImGui::GetIO();
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    GLFWwindow* backup_current_context = glfwGetCurrentContext();
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
    glfwMakeContextCurrent(backup_current_context);
  }
}

void ImGuiLayer::apply_theme(bool dark) {
  ImVec4* colors = ImGui::GetStyle().Colors;

  if (dark) {
    colors[ImGuiCol_Text] = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.42f, 0.42f, 0.42f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_Border] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.34f, 0.34f, 0.34f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.43f, 0.43f, 0.43f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(1.00f, 0.56f, 0.00f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(1.00f, 0.56f, 0.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(1.00f, 0.56f, 0.00f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(1.00f, 0.56f, 0.00f, 0.82f);
    colors[ImGuiCol_Header] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_DockingPreview] = ImVec4(1.00f, 0.56f, 0.00f, 0.22f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 0.56f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.56f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(1.00f, 0.56f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.56f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_TableRowBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.85f, 0.48f, 0.00f, 0.73f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 0.56f, 0.00f, 1.00f);
    colors[ImGuiCol_NavHighlight] = ImVec4(1.00f, 0.56f, 0.00f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);

    header_selected_color = ImVec4(1.00f, 0.56f, 0.00f, 0.50f);
    header_hovered_color = lighten(colors[ImGuiCol_HeaderActive], 0.1f);
    window_bg_color = colors[ImGuiCol_WindowBg];
    window_bg_alternative_color = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
    asset_icon_color = lighten(header_selected_color, 0.9f);
    text_color = colors[ImGuiCol_Text];
    text_disabled_color = colors[ImGuiCol_TextDisabled];
  }
  else {
    colors[ImGuiCol_Text] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.98f);
    colors[ImGuiCol_Border] = ImVec4(0.00f, 0.00f, 0.00f, 0.30f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.70f, 0.82f, 0.95f, 0.39f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.86f, 0.86f, 0.86f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.69f, 0.69f, 0.69f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.49f, 0.49f, 0.49f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.49f, 0.49f, 0.49f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.41f, 0.67f, 0.98f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.70f, 0.82f, 0.95f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.91f, 0.91f, 0.91f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.81f, 0.81f, 0.81f, 0.80f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.89f, 0.89f, 0.89f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.81f, 0.81f, 0.81f, 0.62f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.56f, 0.56f, 0.56f, 0.78f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.78f, 0.78f, 0.78f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.65f, 0.65f, 0.65f, 1.00f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.81f, 0.81f, 0.81f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.91f, 0.91f, 0.91f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.98f, 0.98f, 0.98f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.91f, 0.91f, 0.91f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
    colors[ImGuiCol_DockingPreview] = ImVec4(0.26f, 0.59f, 0.98f, 0.22f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.78f, 0.87f, 0.98f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.57f, 0.57f, 0.64f, 1.00f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.68f, 0.68f, 0.74f, 1.00f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.30f, 0.30f, 0.30f, 0.09f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);

    header_selected_color = ImVec4(0.26f, 0.59f, 0.98f, 0.65f);
    header_hovered_color = darken(colors[ImGuiCol_HeaderActive], 0.1f);
    window_bg_color = colors[ImGuiCol_WindowBg];
    window_bg_alternative_color = darken(window_bg_color, 0.04f);
    asset_icon_color = darken(header_selected_color, 0.9f);
    text_color = colors[ImGuiCol_Text];
    text_disabled_color = colors[ImGuiCol_TextDisabled];
  }
}

void ImGuiLayer::set_style() {
  ImGuiStyle* style = &ImGui::GetStyle();

  style->AntiAliasedFill = true;
  style->AntiAliasedLines = true;
  style->AntiAliasedLinesUseTex = true;

  style->WindowPadding = ImVec2(4.0f, 4.0f);
  style->FramePadding = ImVec2(4.0f, 4.0f);
  style->TabMinWidthForCloseButton = 0.1f;
  style->CellPadding = ImVec2(8.0f, 4.0f);
  style->ItemSpacing = ImVec2(8.0f, 3.0f);
  style->ItemInnerSpacing = ImVec2(2.0f, 4.0f);
  style->TouchExtraPadding = ImVec2(0.0f, 0.0f);
  style->IndentSpacing = 12;
  style->ScrollbarSize = 14;
  style->GrabMinSize = 10;

  style->WindowBorderSize = 0.0f;
  style->ChildBorderSize = 0.0f;
  style->PopupBorderSize = 1.5f;
  style->FrameBorderSize = 0.0f;
  style->TabBorderSize = 0.0f;
  style->DockingSeparatorSize = 0.0f;

  style->WindowRounding = 6.0f;
  style->ChildRounding = 0.0f;
  style->FrameRounding = 2.0f;
  style->PopupRounding = 2.0f;
  style->ScrollbarRounding = 3.0f;
  style->GrabRounding = 2.0f;
  style->LogSliderDeadzone = 4.0f;
  style->TabRounding = 3.0f;

  style->WindowTitleAlign = ImVec2(0.0f, 0.5f);
  style->WindowMenuButtonPosition = ImGuiDir_None;
  style->ColorButtonPosition = ImGuiDir_Left;
  style->ButtonTextAlign = ImVec2(0.5f, 0.5f);
  style->SelectableTextAlign = ImVec2(0.0f, 0.0f);
  style->DisplaySafeAreaPadding = ImVec2(8.0f, 8.0f);

  ui_frame_padding = ImVec2(4.0f, 2.0f);
  popup_item_spacing = ImVec2(6.0f, 8.0f);

  constexpr ImGuiColorEditFlags color_edit_flags = ImGuiColorEditFlags_AlphaBar
                                                   | ImGuiColorEditFlags_AlphaPreviewHalf
                                                   | ImGuiColorEditFlags_DisplayRGB
                                                   | ImGuiColorEditFlags_InputRGB
                                                   | ImGuiColorEditFlags_PickerHueBar
                                                   | ImGuiColorEditFlags_Uint8;
  ImGui::SetColorEditOptions(color_edit_flags);

  style->ScaleAllSizes(1.0f);
}
}
