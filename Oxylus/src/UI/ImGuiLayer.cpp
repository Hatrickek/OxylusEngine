#include "ImGuiLayer.h"
#include <ImGuizmo/ImGuizmo.h>
#include <imgui.h>
#include <plf_colony.h>
#include <backends/imgui_impl_glfw.h>
#include <icons/IconsMaterialDesignIcons.h>
#include <icons/MaterialDesign.inl>
#include <vuk/Pipeline.hpp>
#include <vuk/RenderGraph.hpp>
#include <vuk/Partials.hpp>

#include "imgui_frag.h"
#include "imgui_vert.h"
#include "Core/Application.h"
#include "Core/Resources.h"

#include "Render/Window.h"
#include "Render/Vulkan/VulkanContext.h"
#include "Render/Vulkan/VulkanRenderer.h"
#include "Utils/Profiler.h"

namespace Oxylus {
static ImVec4 Darken(ImVec4 c, float p) { return {glm::max(0.f, c.x - 1.0f * p), glm::max(0.f, c.y - 1.0f * p), glm::max(0.f, c.z - 1.0f * p), c.w}; }
static ImVec4 Lighten(ImVec4 c, float p) { return {glm::max(0.f, c.x + 1.0f * p), glm::max(0.f, c.y + 1.0f * p), glm::max(0.f, c.z + 1.0f * p), c.w}; }

ImFont* ImGuiLayer::RegularFont = nullptr;
ImFont* ImGuiLayer::SmallFont = nullptr;
ImFont* ImGuiLayer::BoldFont = nullptr;

ImGuiLayer::ImGuiLayer() : Layer("ImGuiLayer") { }

void ImGuiLayer::OnAttach(EventDispatcher&) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_ViewportsEnable |
    ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_DpiEnableScaleFonts |
    ImGuiConfigFlags_DpiEnableScaleViewports;
  io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
  io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;

  ImGuiDarkTheme();

  //ImGuizmo style
  {
    ImGuizmo::Style* imguizmoStyle = &ImGuizmo::GetStyle();
    ImVec4* colors = imguizmoStyle->Colors;

    imguizmoStyle->TranslationLineThickness = 3.0f;
    imguizmoStyle->TranslationLineArrowSize = 10.0f;
    imguizmoStyle->RotationLineThickness = 3.0f;
    imguizmoStyle->RotationOuterLineThickness = 5.0f;
    imguizmoStyle->ScaleLineThickness = 3.0f;
    imguizmoStyle->ScaleLineCircleSize = 8.0f;
    imguizmoStyle->HatchedAxisLineThickness = 0.0f;
    imguizmoStyle->CenterCircleSize = 6.0f;

    colors[ImGuizmo::DIRECTION_X] = ImVec4(0.858f, 0.243f, 0.113f, 0.929f);
    colors[ImGuizmo::DIRECTION_Y] = ImVec4(0.375f, 0.825f, 0.372f, 0.929f);
    colors[ImGuizmo::DIRECTION_Z] = ImVec4(0.227f, 0.478f, 0.972f, 0.929f);
    colors[ImGuizmo::PLANE_X] = colors[ImGuizmo::DIRECTION_X];
    colors[ImGuizmo::PLANE_Y] = colors[ImGuizmo::DIRECTION_Y];
    colors[ImGuizmo::PLANE_Z] = colors[ImGuizmo::DIRECTION_Z];
  }

  InitForVulkan();
}

void ImGuiLayer::AddIconFont(float fontSize) {
  const ImGuiIO& io = ImGui::GetIO();
  static constexpr ImWchar icons_ranges[] = {ICON_MIN_MDI, ICON_MAX_MDI, 0};
  ImFontConfig iconsConfig;
  // merge in icons from Font Awesome
  iconsConfig.MergeMode = true;
  iconsConfig.PixelSnapH = true;
  iconsConfig.GlyphOffset.y = 1.0f;
  iconsConfig.OversampleH = iconsConfig.OversampleV = 1;
  iconsConfig.GlyphMinAdvanceX = 4.0f;
  iconsConfig.SizePixels = 12.0f;

  io.Fonts->AddFontFromMemoryCompressedTTF(MaterialDesign_compressed_data,
    MaterialDesign_compressed_size,
    fontSize,
    &iconsConfig,
    icons_ranges);
}

ImGuiLayer::ImGuiData ImGuiLayer::ImGui_ImplVuk_Init(vuk::Allocator& allocator) const {
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


void ImGuiLayer::InitForVulkan() {
  ImGui_ImplGlfw_InitForVulkan(Window::GetGLFWWindow(), true);

  // Upload Fonts
  const auto regularFontPath = Resources::GetResourcesPath("Fonts/jetbrains-mono/JetBrainsMono-Regular.ttf");
  const auto boldFontPath = Resources::GetResourcesPath("Fonts/jetbrains-mono/JetBrainsMono-Bold.ttf");

  const ImGuiIO& io = ImGui::GetIO();
  constexpr float fontSize = 16.0f;
  constexpr float fontSizeSmall = 12.0f;

  ImFontConfig iconsConfig;
  iconsConfig.MergeMode = false;
  iconsConfig.PixelSnapH = true;
  iconsConfig.OversampleH = iconsConfig.OversampleV = 1;
  iconsConfig.GlyphMinAdvanceX = 4.0f;
  iconsConfig.SizePixels = 12.0f;

  RegularFont = io.Fonts->AddFontFromFileTTF(regularFontPath.c_str(), fontSize, &iconsConfig);
  AddIconFont(fontSize);
  SmallFont = io.Fonts->AddFontFromFileTTF(regularFontPath.c_str(), fontSizeSmall, &iconsConfig);
  AddIconFont(fontSizeSmall);
  BoldFont = io.Fonts->AddFontFromFileTTF(boldFontPath.c_str(), fontSize, &iconsConfig);
  AddIconFont(fontSize);

  io.Fonts->TexGlyphPadding = 1;
  for (int n = 0; n < io.Fonts->ConfigData.Size; n++) {
    ImFontConfig* fontConfig = &io.Fonts->ConfigData[n];
    fontConfig->RasterizerMultiply = 1.0f;
  }
  io.Fonts->Build();

  ImGuiStyle* style = &ImGui::GetStyle();
  style->WindowMenuButtonPosition = ImGuiDir_None;

  uint8_t* pixels;
  int32_t width, height;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

  m_ImGuiData = ImGui_ImplVuk_Init(*VulkanContext::Get()->superframe_allocator);
}

void ImGuiLayer::OnDetach() {
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}

void ImGuiLayer::Begin() {
  OX_SCOPED_ZONE;
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  m_SampledImages.clear();
}

vuk::Future ImGuiLayer::RenderDrawData(vuk::Allocator& allocator, vuk::Future target, ImDrawData* drawData) const {
  OX_SCOPED_ZONE;
  auto reset_render_state = [](const ImGuiData& data, vuk::CommandBuffer& command_buffer, ImDrawData* draw_data, vuk::Buffer vertex, vuk::Buffer index) {
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

  size_t vertex_size = drawData->TotalVtxCount * sizeof(ImDrawVert);
  size_t index_size = drawData->TotalIdxCount * sizeof(ImDrawIdx);
  auto imvert = *allocate_buffer(allocator, {vuk::MemoryUsage::eCPUtoGPU, vertex_size, 1});
  auto imind = *allocate_buffer(allocator, {vuk::MemoryUsage::eCPUtoGPU, index_size, 1});

  size_t vtx_dst = 0, idx_dst = 0;
  vuk::Compiler comp;
  for (int n = 0; n < drawData->CmdListsCount; n++) {
    const ImDrawList* cmd_list = drawData->CmdLists[n];
    auto imverto = imvert->add_offset(vtx_dst * sizeof(ImDrawVert));
    auto imindo = imind->add_offset(idx_dst * sizeof(ImDrawIdx));

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
  for (auto& si : m_SampledImages) {
    if (!si.is_global) {
      resources.emplace_back(si.rg_attachment.reference.rg, si.rg_attachment.reference.name, vuk::Resource::Type::eImage, vuk::Access::eFragmentSampled);
    }
  }
  vuk::Pass pass{
    .name = "imgui",
    .resources = std::move(resources),
    .execute = [this, &allocator, verts = imvert.get(), inds = imind.get(), reset_render_state, drawData](vuk::CommandBuffer& command_buffer) {
      command_buffer.set_dynamic_state(vuk::DynamicStateFlagBits::eViewport | vuk::DynamicStateFlagBits::eScissor);
      command_buffer.set_rasterization(vuk::PipelineRasterizationStateCreateInfo{});
      command_buffer.set_color_blend("target", vuk::BlendPreset::eAlphaBlend);
      reset_render_state(m_ImGuiData, command_buffer, drawData, verts, inds);
      // Will project scissor/clipping rectangles into framebuffer space
      ImVec2 clip_off = drawData->DisplayPos;         // (0,0) unless using multi-viewports
      ImVec2 clip_scale = drawData->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

      // Render command lists
      // (Because we merged all buffers into a single one, we maintain our own offset into them)
      int global_vtx_offset = 0;
      int global_idx_offset = 0;
      for (int n = 0; n < drawData->CmdListsCount; n++) {
        const ImDrawList* cmd_list = drawData->CmdLists[n];
        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
          const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
          if (pcmd->UserCallback != nullptr) {
            // User callback, registered via ImDrawList::AddCallback()
            // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
            if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
              reset_render_state(m_ImGuiData, command_buffer, drawData, verts, inds);
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

            auto fb_width = command_buffer.get_ongoing_render_pass().extent.width;
            auto fb_height = command_buffer.get_ongoing_render_pass().extent.height;
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
                    auto res_img = command_buffer.get_resource_image_attachment(si.rg_attachment.reference)->image;
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

vuk::SampledImage* ImGuiLayer::AddSampledImage(const vuk::SampledImage& sampledImage) {
  return &*m_SampledImages.emplace(sampledImage);
}

void ImGuiLayer::End() {
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

void ImGuiLayer::SetTheme(int index) {
  SelectedTheme = index;
  switch (index) {
    case 0: ImGuiDarkTheme();
      break;
    case 1: ImGui::StyleColorsLight();
      break;
    default: ImGuiDarkTheme();
      break;
  }
}

void ImGuiLayer::ImGuiDarkTheme() {
  ImGuiStyle* style = &ImGui::GetStyle();
  ImVec4* colors = style->Colors;
  style->WindowBorderSize = 0;
  style->WindowRounding = 3.0f;
  style->FrameRounding = 2.0f;
  style->TabRounding = 3.0f;
  style->GrabRounding = 1.0f;
  style->FramePadding = {8, 4};
#pragma region DARK_THEME
  colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
  colors[ImGuiCol_Tab] = ImVec4(0.380f, 0.380f, 0.380f, 1.000f);
  colors[ImGuiCol_TabHovered] = ImVec4(0.584f, 0.584f, 0.584f, 1.000f);
  colors[ImGuiCol_TabActive] = ImVec4(0.519f, 0.519f, 0.519f, 1.000f);
  colors[ImGuiCol_TabUnfocused] = ImVec4(0.130f, 0.130f, 0.130f, 1.000f);
  colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.381f, 0.381f, 0.381f, 1.000f);
  colors[ImGuiCol_Header] = ImVec4(0.8f, 0.180f, 0.000f, 1.000f);
  colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
  colors[ImGuiCol_PopupBg] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
  colors[ImGuiCol_BorderShadow] = ImVec4(0.92f, 0.91f, 0.88f, 0.00f);
  colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
  colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
  colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.00f, 0.98f, 0.95f, 0.75f);
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
  colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
  colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
  colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
  colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
  colors[ImGuiCol_CheckMark] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
  colors[ImGuiCol_SliderGrab] = ImVec4(0.8f, 0.180f, 0.000f, 1.000f);
  colors[ImGuiCol_SliderGrabActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
  colors[ImGuiCol_Button] = ImVec4(0.87f, 0.40f, 0.00f, 1.00f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.970f, 0.448f, 0.000f, 1.000f);
  colors[ImGuiCol_ButtonActive] = ImVec4(1.000f, 0.234f, 0.000f, 1.000f);
  colors[ImGuiCol_Header] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
  colors[ImGuiCol_ResizeGrip] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
  colors[ImGuiCol_ResizeGripActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
  colors[ImGuiCol_PlotLines] = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
  colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
  colors[ImGuiCol_PlotHistogram] = ImVec4(1.000f, 0.271f, 0.000f, 0.631f);
  colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
  colors[ImGuiCol_TextSelectedBg] = ImVec4(0.25f, 1.00f, 0.00f, 0.43f);
  colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.73f);
  colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0, 0, 0, 0.73f);
  colors[ImGuiCol_Border] = ImVec4(0.118f, 0.118f, 0.118f, 0.878f);
  colors[ImGuiCol_Separator] = ImVec4(0.118f, 0.118f, 0.118f, 0.878f);
  HeaderSelectedColor = ImVec4(0.19f, 0.53f, 0.78f, 1.00f);
  HeaderHoveredColor = Lighten(colors[ImGuiCol_HeaderActive], 0.1f);
  WindowBgColor = colors[ImGuiCol_WindowBg];
  WindowBgAlternativeColor = Lighten(WindowBgColor, 0.04f);
  AssetIconColor = Lighten(HeaderSelectedColor, 0.9f);
  TextColor = colors[ImGuiCol_Text];
  TextDisabledColor = colors[ImGuiCol_TextDisabled];
#pragma endregion
  SelectedTheme = 0;
}
}
