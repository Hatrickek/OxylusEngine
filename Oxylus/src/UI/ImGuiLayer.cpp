#include "src/oxpch.h"
#include "ImGuiLayer.h"
#include <ImGuizmo/ImGuizmo.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <icons/IconsMaterialDesignIcons.h>
#include <icons/MaterialDesign.inl>
#include "Render/Window.h"
#include "Render/Vulkan/VulkanContext.h"
#include "Render/Vulkan/VulkanRenderer.h"
#include "Render/Vulkan/Utils/VulkanUtils.h"
#include "Render/Vulkan/VulkanSwapchain.h"
#include "Utils/Profiler.h"

namespace Oxylus {
  static ImVec4 Darken(ImVec4 c, float p) { return {glm::max(0.f, c.x - 1.0f * p), glm::max(0.f, c.y - 1.0f * p), glm::max(0.f, c.z - 1.0f * p), c.w}; }
  static ImVec4 Lighten(ImVec4 c, float p) { return {glm::max(0.f, c.x + 1.0f * p), glm::max(0.f, c.y + 1.0f * p), glm::max(0.f, c.z + 1.0f * p), c.w}; }

  ImFont* ImGuiLayer::RegularFont = nullptr;
  ImFont* ImGuiLayer::SmallFont = nullptr;
  ImFont* ImGuiLayer::BoldFont = nullptr;
  static vk::DescriptorSet s_FontDescriptorSet;

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

  void ImGuiLayer::InitForVulkan() {
    const auto& LogicalDevice = VulkanContext::Context.Device;
    VkDescriptorPool ImGuiDescPool;
    constexpr VkDescriptorPoolSize poolSizes[] = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},};
    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 2;
    poolInfo.poolSizeCount = (uint32_t)IM_ARRAYSIZE(poolSizes);
    poolInfo.pPoolSizes = poolSizes;
    VulkanUtils::CheckResult(vkCreateDescriptorPool(LogicalDevice, &poolInfo, nullptr, &ImGuiDescPool));

    ImGui_ImplGlfw_InitForVulkan(Window::GetGLFWWindow(), true);

    // Upload Fonts
    constexpr const char* regularFontPath = "Resources/Fonts/jetbrains-mono/JetBrainsMono-Regular.ttf";
    constexpr const char* boldFontPath = "Resources/Fonts/jetbrains-mono/JetBrainsMono-Bold.ttf";

    const ImGuiIO& io = ImGui::GetIO();
    constexpr float fontSize = 16.0f;
    constexpr float fontSizeSmall = 12.0f;

    ImFontConfig iconsConfig;
    iconsConfig.MergeMode = false;
    iconsConfig.PixelSnapH = true;
    iconsConfig.OversampleH = iconsConfig.OversampleV = 1;
    iconsConfig.GlyphMinAdvanceX = 4.0f;
    iconsConfig.SizePixels = 12.0f;

    RegularFont = io.Fonts->AddFontFromFileTTF(regularFontPath, fontSize, &iconsConfig);
    AddIconFont(fontSize);
    SmallFont = io.Fonts->AddFontFromFileTTF(regularFontPath, fontSizeSmall, &iconsConfig);
    AddIconFont(fontSizeSmall);
    BoldFont = io.Fonts->AddFontFromFileTTF(boldFontPath, fontSize, &iconsConfig);
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
    const size_t upload_size = width * height * 4 * sizeof(char);

    VulkanImageDescription fontImageDesc;
    fontImageDesc.Width = width;
    fontImageDesc.Height = height;
    fontImageDesc.EmbeddedStbData = pixels;
    fontImageDesc.EmbeddedDataLength = upload_size;
    fontImageDesc.CreateView = true;
    fontImageDesc.CreateSampler = true;
    m_FontImage.Create(fontImageDesc);

    vk::DescriptorSetAllocateInfo allocInfo = {};
    allocInfo.descriptorPool = DescriptorPoolManager::Get()->GetFreePool();
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &VulkanRenderer::s_Pipelines.UIPipeline.GetDescriptorSetLayout()[0];
    VulkanUtils::CheckResult(LogicalDevice.allocateDescriptorSets(&allocInfo, &s_FontDescriptorSet));
    vk::DescriptorImageInfo descImage[1] = {};
    descImage[0].sampler = m_FontImage.GetImageSampler();
    descImage[0].imageView = m_FontImage.GetImageView();
    descImage[0].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    vk::WriteDescriptorSet writeDesc[1] = {};
    writeDesc[0].dstSet = s_FontDescriptorSet;
    writeDesc[0].descriptorCount = 1;
    writeDesc[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    writeDesc[0].pImageInfo = descImage;
    LogicalDevice.updateDescriptorSets(1, writeDesc, 0, nullptr);

    io.Fonts->SetTexID(s_FontDescriptorSet);

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = VulkanContext::Context.Instance;
    init_info.PhysicalDevice = VulkanContext::Context.PhysicalDevice;
    init_info.Device = LogicalDevice;
    init_info.QueueFamily = VulkanContext::VulkanQueue.graphicsQueueFamilyIndex;
    init_info.Queue = VulkanContext::VulkanQueue.GraphicsQueue;
    init_info.DescriptorPool = ImGuiDescPool;
    init_info.MinImageCount = static_cast<uint32_t>(VulkanRenderer::SwapChain.m_Images.size());
    init_info.ImageCount = static_cast<uint32_t>(VulkanRenderer::SwapChain.m_Images.size());
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    ImGui_ImplVulkan_Init(&init_info, VulkanRenderer::SwapChain.m_RenderPass.Get());
  }

  void ImGuiLayer::OnDetach() {
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
  }

  void ImGuiLayer::Begin() {
    OX_SCOPED_ZONE;
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
  }

  void ImGuiLayer::RenderDrawData(const vk::CommandBuffer& commandBuffer,
                                  const vk::Pipeline& pipeline) const {
    OX_SCOPED_ZONE;
    if (!m_DrawData || m_DrawData->CmdListsCount == 0)
      return;
    ImGui_ImplVulkan_RenderDrawData(m_DrawData, commandBuffer, pipeline);
  }

  void ImGuiLayer::End() {
    OX_SCOPED_ZONE;
    ImGui::Render();
    m_DrawData = ImGui::GetDrawData();

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
