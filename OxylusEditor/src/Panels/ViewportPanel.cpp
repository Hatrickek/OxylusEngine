#include "ViewportPanel.h"

#include <icons/IconsMaterialDesignIcons.h>
#include <vuk/CommandBuffer.hpp>
#include <vuk/RenderGraph.hpp>

#include "EditorLayer.h"
#include "ImGuizmo.h"
#include "glm/gtc/type_ptr.hpp"

#include "Render/DebugRenderer.h"
#include "Render/RendererConfig.h"
#include "Render/RenderPipeline.h"
#include "Render/Vulkan/Renderer.h"
#include "Render/Vulkan/VulkanContext.h"

#include "Scene/SceneRenderer.h"

#include "UI/OxUI.h"

#include "Utils/FileUtils.h"
#include "Utils/OxMath.h"
#include "Utils/StringUtils.h"
#include "Utils/Timestep.h"

namespace Oxylus {
ViewportPanel::ViewportPanel() : EditorPanel("Viewport", ICON_MDI_TERRAIN, true) {}

void ViewportPanel::on_imgui_render() {
  draw_performance_overlay();

  bool viewportSettingsPopup = false;
  constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar;
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});

  if (on_begin(flags)) {
    ImVec2 startCursorPos = ImGui::GetCursorPos();

    const auto popupItemSpacing = ImGuiLayer::PopupItemSpacing;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, popupItemSpacing);
    if (ImGui::BeginPopupContextItem("RightClick")) {
      if (ImGui::MenuItem("Fullscreen"))
        FullscreenViewport = !FullscreenViewport;
      ImGui::EndPopup();
    }
    ImGui::PopStyleVar();

    if (ImGui::BeginMenuBar()) {
      if (ImGui::MenuItem(StringUtils::from_char8_t(ICON_MDI_COGS))) {
        viewportSettingsPopup = true;
      }
      ImGui::EndMenuBar();
    }
    ImGui::PopStyleVar();

    if (viewportSettingsPopup)
      ImGui::OpenPopup("ViewportSettings");

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
    if (ImGui::BeginPopup("ViewportSettings")) {
      OxUI::begin_properties();
      static bool vsync = VulkanContext::get()->present_mode == vuk::PresentModeKHR::eFifo ? true : false;
      if (OxUI::property("VSync", &vsync)) {
        VulkanContext::get()->rebuild_swapchain();
        RendererCVar::cvar_vsync.set(vsync);
      }
      OxUI::property<float>("Camera sensitivity", &m_MouseSensitivity, 0.1f, 20.0f);
      OxUI::property<float>("Movement speed", &m_MovementSpeed, 5, 100.0f);
      OxUI::property("Smooth camera", &m_SmoothCamera);
      OxUI::end_properties();
      ImGui::EndPopup();
    }

    const auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
    const auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
    viewport_offset = ImGui::GetWindowPos();
    m_ViewportBounds[0] = {viewportMinRegion.x + viewport_offset.x, viewportMinRegion.y + viewport_offset.y};
    m_ViewportBounds[1] = {viewportMaxRegion.x + viewport_offset.x, viewportMaxRegion.y + viewport_offset.y};

    m_ViewportFocused = ImGui::IsWindowFocused();
    m_ViewportHovered = ImGui::IsWindowHovered();

    viewport_panel_size = ImGui::GetContentRegionAvail();
    if ((int)m_ViewportSize.x != (int)viewport_panel_size.x || (int)m_ViewportSize.y != (int)viewport_panel_size.y) {
      m_ViewportSize = {viewport_panel_size.x, viewport_panel_size.y};
    }

    constexpr auto sixteenNineAR = 1.7777777f;
    const auto fixedWidth = m_ViewportSize.y * sixteenNineAR;
    ImGui::SetCursorPosX((viewport_panel_size.x - fixedWidth) * 0.5f);

    const auto dim = vuk::Dimension3D::absolute((uint32_t)fixedWidth, (uint32_t)viewport_panel_size.y);
    const auto rp = m_Scene->get_renderer()->get_render_pipeline();
    rp->detach_swapchain(dim);
    const auto final_image = rp->get_final_image();

    if (final_image) {
      OxUI::image(*final_image, ImVec2{fixedWidth, m_ViewportSize.y});
    }
    else {
      const auto textWidth = ImGui::CalcTextSize("No render target!").x;
      ImGui::SetCursorPosX((m_ViewportSize.x - textWidth) * 0.5f);
      ImGui::SetCursorPosY(m_ViewportSize.y * 0.5f);
      ImGui::Text("No render target!");
    }

    if (scene_hierarchy_panel)
      scene_hierarchy_panel->drag_drop_target();

    draw_gizmos();

    {
      // Transform Gizmos Button Group
      const float frameHeight = 1.3f * ImGui::GetFrameHeight();
      const ImVec2 framePadding = ImGui::GetStyle().FramePadding;
      const ImVec2 buttonSize = {frameHeight, frameHeight};
      constexpr float buttonCount = 7.0f;
      const ImVec2 gizmoPosition = {m_ViewportBounds[0].x + m_GizmoPosition.x, m_ViewportBounds[0].y + m_GizmoPosition.y};
      const ImRect bb(gizmoPosition.x, gizmoPosition.y, gizmoPosition.x + buttonSize.x + 8, gizmoPosition.y + (buttonSize.y + 2) * (buttonCount + 0.5f));
      ImVec4 frameColor = ImGui::GetStyleColorVec4(ImGuiCol_Tab);
      frameColor.w = 0.5f;
      ImGui::RenderFrame(bb.Min, bb.Max, ImGui::GetColorU32(frameColor), false, ImGui::GetStyle().FrameRounding);
      const ImVec2 tempGizmoPosition = m_GizmoPosition;

      ImGui::SetCursorPos({startCursorPos.x + tempGizmoPosition.x + framePadding.x, startCursorPos.y + tempGizmoPosition.y});
      ImGui::BeginGroup();
      {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {1, 1});

        const ImVec2 draggerCursorPos = ImGui::GetCursorPos();
        ImGui::SetCursorPosX(draggerCursorPos.x + framePadding.x);
        ImGui::TextUnformatted(StringUtils::from_char8_t(ICON_MDI_DOTS_HORIZONTAL));
        ImVec2 draggerSize = ImGui::CalcTextSize(StringUtils::from_char8_t(ICON_MDI_DOTS_HORIZONTAL));
        draggerSize.x *= 2.0f;
        ImGui::SetCursorPos(draggerCursorPos);
        ImGui::InvisibleButton("GizmoDragger", draggerSize);
        static ImVec2 lastMousePosition = ImGui::GetMousePos();
        const ImVec2 mousePos = ImGui::GetMousePos();
        if (ImGui::IsItemActive()) {
          m_GizmoPosition.x += mousePos.x - lastMousePosition.x;
          m_GizmoPosition.y += mousePos.y - lastMousePosition.y;
        }
        lastMousePosition = mousePos;

        constexpr float alpha = 0.6f;
        if (OxUI::toggle_button(StringUtils::from_char8_t(ICON_MDI_AXIS_ARROW), m_GizmoType == ImGuizmo::TRANSLATE, buttonSize, alpha, alpha))
          m_GizmoType = ImGuizmo::TRANSLATE;
        if (OxUI::toggle_button(StringUtils::from_char8_t(ICON_MDI_ROTATE_3D), m_GizmoType == ImGuizmo::ROTATE, buttonSize, alpha, alpha))
          m_GizmoType = ImGuizmo::ROTATE;
        if (OxUI::toggle_button(StringUtils::from_char8_t(ICON_MDI_ARROW_EXPAND), m_GizmoType == ImGuizmo::SCALE, buttonSize, alpha, alpha))
          m_GizmoType = ImGuizmo::SCALE;
        if (OxUI::toggle_button(StringUtils::from_char8_t(ICON_MDI_VECTOR_SQUARE), m_GizmoType == ImGuizmo::BOUNDS, buttonSize, alpha, alpha))
          m_GizmoType = ImGuizmo::BOUNDS;
        if (OxUI::toggle_button(StringUtils::from_char8_t(ICON_MDI_ARROW_EXPAND_ALL), m_GizmoType == ImGuizmo::UNIVERSAL, buttonSize, alpha, alpha))
          m_GizmoType = ImGuizmo::UNIVERSAL;
        if (OxUI::toggle_button(m_GizmoMode == ImGuizmo::WORLD ? StringUtils::from_char8_t(ICON_MDI_EARTH) : StringUtils::from_char8_t(ICON_MDI_EARTH_OFF), m_GizmoMode == ImGuizmo::WORLD, buttonSize, alpha, alpha))
          m_GizmoMode = m_GizmoMode == ImGuizmo::LOCAL ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
        if (OxUI::toggle_button(StringUtils::from_char8_t(ICON_MDI_GRID), RendererCVar::cvar_draw_grid.get(), buttonSize, alpha, alpha))
          RendererCVar::cvar_draw_grid.toggle();

        ImGui::PopStyleVar(2);
      }
      ImGui::EndGroup();
    }

    {
      // Scene Button Group
      const float frameHeight = 1.0f * ImGui::GetFrameHeight();
      const ImVec2 buttonSize = {frameHeight, frameHeight};
      constexpr float buttonCount = 3.0f;
      const ImVec2 gizmoPosition = {m_ViewportBounds[0].x + startCursorPos.x + m_ViewportSize.x * 0.5f - 8.0f, m_ViewportBounds[0].y + 8.0f};
      const auto width = gizmoPosition.x + (buttonSize.x + 12) * (buttonCount + 0.5f);
      const ImRect bb(gizmoPosition.x, gizmoPosition.y, width, gizmoPosition.y + buttonSize.y + 8);
      ImVec4 frameColor = ImGui::GetStyleColorVec4(ImGuiCol_Tab);
      frameColor.w = 0.5f;
      ImGui::RenderFrame(bb.Min, bb.Max, ImGui::GetColorU32(frameColor), false, 3.0f);

      ImGui::SetCursorPos({startCursorPos.x + m_ViewportSize.x * 0.5f, startCursorPos.y + ImGui::GetStyle().FramePadding.y + 8.0f});
      ImGui::BeginGroup();
      {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {1, 1});

        const ImVec2 buttonSize2 = {frameHeight * 1.5f, frameHeight};
        const bool highlight = EditorLayer::get()->scene_state == EditorLayer::SceneState::Play;
        const char8_t* icon = EditorLayer::get()->scene_state == EditorLayer::SceneState::Edit ? ICON_MDI_PLAY : ICON_MDI_STOP;
        if (OxUI::toggle_button(StringUtils::from_char8_t(icon), highlight, buttonSize2)) {
          if (EditorLayer::get()->scene_state == EditorLayer::SceneState::Edit)
            EditorLayer::get()->on_scene_play();
          else if (EditorLayer::get()->scene_state == EditorLayer::SceneState::Play)
            EditorLayer::get()->on_scene_stop();
        }
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 0.4f));
        if (ImGui::Button(StringUtils::from_char8_t(ICON_MDI_PAUSE), buttonSize2)) {
          if (EditorLayer::get()->scene_state == EditorLayer::SceneState::Play)
            EditorLayer::get()->on_scene_stop();
        }
        ImGui::SameLine();
        if (ImGui::Button(StringUtils::from_char8_t(ICON_MDI_STEP_FORWARD), buttonSize2)) {
          EditorLayer::get()->on_scene_simulate();
        }
        ImGui::PopStyleColor();

        ImGui::PopStyleVar(2);
      }
      ImGui::EndGroup();
    }

    ImGui::PopStyleVar();
    on_end();
  }
}

void ViewportPanel::set_context(const Ref<Scene>& scene, SceneHierarchyPanel& sceneHierarchyPanel) {
  scene_hierarchy_panel = &sceneHierarchyPanel;
  m_Scene = scene;
}

void ViewportPanel::on_update() {
  // entity highlighting
  if (scene_hierarchy_panel) {
    auto entity = scene_hierarchy_panel->get_selected_entity();
    if (entity && entity.has_component<MeshComponent>()) {
      auto scaled_matrix = glm::scale(entity.get_world_transform(), Vec3(1.02f));
      DebugRenderer::draw_mesh(
        entity.get_component<MeshComponent>().original_mesh,
        scaled_matrix,
        Vec4(0, 1, 0, 1)
      );
    }
  }

  if (m_ViewportFocused && !m_SimulationRunning && m_UseEditorCamera) {
    const Vec3& position = m_camera.get_position();
    const glm::vec2 yawPitch = glm::vec2(m_camera.get_yaw(), m_camera.get_pitch());
    Vec3 finalPosition = position;
    glm::vec2 finalYawPitch = yawPitch;

    if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
      const glm::vec2 newMousePosition = Input::GetMousePosition();

      if (!m_UsingEditorCamera) {
        m_UsingEditorCamera = true;
        m_LockedMousePosition = newMousePosition;
      }

      Input::SetCursorPosition(m_LockedMousePosition.x, m_LockedMousePosition.y);
      //Input::SetCursorIcon(EditorLayer::Get()->m_CrosshairCursor);

      const glm::vec2 change = (newMousePosition - m_LockedMousePosition) * m_MouseSensitivity;
      finalYawPitch.x += change.x;
      finalYawPitch.y = glm::clamp(finalYawPitch.y - change.y, glm::radians(-89.9f), glm::radians(89.9f));

      const float maxMoveSpeed = m_MovementSpeed * (ImGui::IsKeyDown(ImGuiKey_LeftShift) ? 3.0f : 1.0f);
      if (ImGui::IsKeyDown(ImGuiKey_W))
        finalPosition += m_camera.get_front() * maxMoveSpeed;
      else if (ImGui::IsKeyDown(ImGuiKey_S))
        finalPosition -= m_camera.get_front() * maxMoveSpeed;
      if (ImGui::IsKeyDown(ImGuiKey_D))
        finalPosition += m_camera.get_right() * maxMoveSpeed;
      else if (ImGui::IsKeyDown(ImGuiKey_A))
        finalPosition -= m_camera.get_right() * maxMoveSpeed;

      if (ImGui::IsKeyDown(ImGuiKey_Q)) {
        finalPosition.y -= maxMoveSpeed;
      }
      else if (ImGui::IsKeyDown(ImGuiKey_E)) {
        finalPosition.y += maxMoveSpeed;
      }
    }
    // Panning
    else if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
      const glm::vec2 newMousePosition = Input::GetMousePosition();

      if (!m_UsingEditorCamera) {
        m_UsingEditorCamera = true;
        m_LockedMousePosition = newMousePosition;
      }

      Input::SetCursorPosition(m_LockedMousePosition.x, m_LockedMousePosition.y);
      //Input::SetCursorIcon(EditorLayer::Get()->m_CrosshairCursor);

      const glm::vec2 change = (newMousePosition - m_LockedMousePosition) * m_MouseSensitivity;

      const float maxMoveSpeed = m_MovementSpeed * (ImGui::IsKeyDown(ImGuiKey_LeftShift) ? 3.0f : 1.0f);
      finalPosition += m_camera.get_front() * change.y * maxMoveSpeed;
      finalPosition += m_camera.get_right() * change.x * maxMoveSpeed;
    }
    else {
      //Input::SetCursorIconDefault();
      m_UsingEditorCamera = false;
    }

    const Vec3 dampedPosition = Math::smooth_damp(position,
      finalPosition,
      m_TranslationVelocity,
      m_TranslationDampening,
      10000.0f,
      Application::get_timestep().get_seconds());
    const glm::vec2 dampedYawPitch = Math::smooth_damp(yawPitch,
      finalYawPitch,
      m_RotationVelocity,
      m_RotationDampening,
      1000.0f,
      Application::get_timestep().get_seconds());

    m_camera.set_position(m_SmoothCamera ? dampedPosition : finalPosition);
    m_camera.set_yaw(m_SmoothCamera ? dampedYawPitch.x : finalYawPitch.x);
    m_camera.set_pitch(m_SmoothCamera ? dampedYawPitch.y : finalYawPitch.y);

    m_camera.update();
  }
}

void ViewportPanel::draw_performance_overlay() {
  if (!performance_overlay_visible)
    return;
  OxUI::draw_framerate_overlay(viewport_offset, viewport_panel_size, {15, 55}, &performance_overlay_visible);
}

void ViewportPanel::draw_gizmos() {
  const Entity selectedEntity = scene_hierarchy_panel->get_selected_entity();
  if (selectedEntity && m_GizmoType != -1) {
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(m_ViewportBounds[0].x,
      m_ViewportBounds[0].y,
      m_ViewportBounds[1].x - m_ViewportBounds[0].x,
      m_ViewportBounds[1].y - m_ViewportBounds[0].y);

    const glm::mat4& cameraProjection = m_camera.get_projection_matrix();
    const glm::mat4& cameraView = m_camera.get_view_matrix();

    auto& tc = selectedEntity.get_component<TransformComponent>();
    glm::mat4 transform = selectedEntity.get_world_transform();

    // Snapping
    const bool snap = Input::GetKey(Key::LeftControl);
    float snapValue = 0.5f; // Snap to 0.5m for translation/scale
    // Snap to 45 degrees for rotation
    if (m_GizmoType == ImGuizmo::OPERATION::ROTATE)
      snapValue = 45.0f;

    const float snapValues[3] = {snapValue, snapValue, snapValue};

    ImGuizmo::Manipulate(glm::value_ptr(cameraView),
      glm::value_ptr(cameraProjection),
      static_cast<ImGuizmo::OPERATION>(m_GizmoType),
      static_cast<ImGuizmo::MODE>(m_GizmoMode),
      glm::value_ptr(transform),
      nullptr,
      snap ? snapValues : nullptr);

    if (ImGuizmo::IsUsing()) {
      const Entity parent = selectedEntity.get_parent();
      const glm::mat4& parentWorldTransform = parent ? parent.get_world_transform() : glm::mat4(1.0f);
      Vec3 translation, rotation, scale;
      if (Math::decompose_transform(glm::inverse(parentWorldTransform) * transform, translation, rotation, scale)) {
        tc.position = translation;
        const Vec3 deltaRotation = rotation - tc.rotation;
        tc.rotation += deltaRotation;
        tc.scale = scale;
      }
    }
  }
  if (Input::GetKey(Key::LeftControl)) {
    if (Input::GetKeyDown(Key::Q)) {
      if (!ImGuizmo::IsUsing())
        m_GizmoType = -1;
    }
    if (Input::GetKeyDown(Key::W)) {
      if (!ImGuizmo::IsUsing())
        m_GizmoType = ImGuizmo::OPERATION::TRANSLATE;
    }
    if (Input::GetKeyDown(Key::E)) {
      if (!ImGuizmo::IsUsing())
        m_GizmoType = ImGuizmo::OPERATION::ROTATE;
    }
    if (Input::GetKeyDown(Key::R)) {
      if (!ImGuizmo::IsUsing())
        m_GizmoType = ImGuizmo::OPERATION::SCALE;
    }
  }
}
}
