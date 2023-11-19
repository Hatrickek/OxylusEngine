#include "EditorLayer.h"

#include <filesystem>
#include <icons/IconsMaterialDesignIcons.h>

#include <imgui_internal.h>
#include "Assets/AssetManager.h"
#include "Core/Input.h"
#include "Core/Project.h"
#include "Core/Resources.h"
#include "Panels/ConsolePanel.h"
#include "Panels/ContentPanel.h"
#include "Panels/EditorDebugPanel.h"
#include "Panels/EditorSettingsPanel.h"
#include "Panels/FramebufferViewerPanel.h"
#include "Panels/ProjectPanel.h"
#include "Panels/RendererSettingsPanel.h"
#include "Panels/SceneHierarchyPanel.h"
#include "Panels/ShadersPanel.h"
#include "Panels/StatisticsPanel.h"
#include "Render/Window.h"
#include "Render/Vulkan/Renderer.h"

#include "Scene/SceneRenderer.h"

#include "UI/OxUI.h"
#include "Utils/EditorConfig.h"
#include "Utils/ImGuiScoped.h"
#include "Utils/UIUtils.h"

#include "Scene/SceneSerializer.h"

#include "Thread/ThreadManager.h"

#include "Utils/CVars.h"
#include "Utils/EmbeddedBanner.h"
#include "Utils/StringUtils.h"

namespace Oxylus {
EditorLayer* EditorLayer::s_instance = nullptr;

AutoCVar_Int cvar_show_style_editor("ui.imgui_style_editor", "show imgui style editor", 0, CVarFlags::EditCheckbox);
AutoCVar_Int cvar_show_imgui_demo("ui.imgui_demo", "show imgui demo window", 0, CVarFlags::EditCheckbox);

EditorLayer::EditorLayer() : Layer("Editor Layer") {
  s_instance = this;
}

void EditorLayer::on_attach(EventDispatcher& dispatcher) {
  m_editor_config.load_config();
  Resources::init_editor_resources();

  crosshair_cursor = Input::LoadCursorIconStandard(GLFW_CROSSHAIR_CURSOR);

  engine_banner = create_ref<TextureAsset>();
  engine_banner->create_texture(EngineBannerWidth, EngineBannerHeight, EngineBanner);

  Input::SetCursorState(Input::CursorState::NORMAL, Window::get_glfw_window());

  m_editor_scene = create_ref<Scene>();
  set_editor_context(m_editor_scene);

  // Initialize panels
  m_editor_panels.emplace("EditorSettings", create_scope<EditorSettingsPanel>());
  m_editor_panels.emplace("RenderSettings", create_scope<RendererSettingsPanel>());
  m_editor_panels.emplace("Shaders", create_scope<ShadersPanel>());
  m_editor_panels.emplace("FramebufferViewer", create_scope<FramebufferViewerPanel>());
  m_editor_panels.emplace("ProjectPanel", create_scope<ProjectPanel>());
  m_editor_panels.emplace("StatisticsPanel", create_scope<StatisticsPanel>());
  m_editor_panels.emplace("EditorDebugPanel", create_scope<EditorDebugPanel>());
  const auto& viewport = m_viewport_panels.emplace_back(create_scope<ViewportPanel>());
  viewport->m_camera.set_position({-2, 2, 0});
  viewport->set_context(m_editor_scene, m_scene_hierarchy_panel);
}

void EditorLayer::on_detach() {
  Input::DestroyCursor(crosshair_cursor);
  m_editor_config.save_config();
}

void EditorLayer::on_update(const Timestep& delta_time) {
  for (const auto& [name, panel] : m_editor_panels) {
    if (!panel->Visible)
      continue;
    panel->on_update();
  }
  for (const auto& panel : m_viewport_panels) {
    if (!panel->Visible)
      continue;
    panel->on_update();
  }
  if (m_scene_hierarchy_panel.Visible)
    m_scene_hierarchy_panel.on_update();
  if (m_content_panel.Visible)
    m_content_panel.on_update();
  if (m_inspector_panel.Visible)
    m_inspector_panel.on_update();

  m_asset_inspector_panel.on_update();

  switch (scene_state) {
    case SceneState::Edit: {
      m_editor_scene->on_editor_update(delta_time, m_viewport_panels[0]->m_camera);
      break;
    }
    case SceneState::Play: {
      m_active_scene->on_runtime_update(delta_time);
      break;
    }
    case SceneState::Simulate: {
      m_active_scene->on_editor_update(delta_time, m_viewport_panels[0]->m_camera);
      break;
    }
  }
}

void EditorLayer::on_imgui_render() {
  if (cvar_show_style_editor.get())
    ImGui::ShowStyleEditor();
  if (cvar_show_imgui_demo.get())
    ImGui::ShowDemoWindow();

  editor_shortcuts();

  constexpr ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;

  constexpr ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
                                            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNavFocus |
                                            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
                                            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings;

  ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::SetNextWindowViewport(viewport->ID);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  if (ImGui::Begin("DockSpace", nullptr, window_flags)) {
    ImGui::PopStyleVar(3);

    // Submit the DockSpace
    const ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
      dockspace_id = ImGui::GetID("MainDockspace");
      ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
    }

    const float frame_height = ImGui::GetFrameHeight();

    constexpr ImGuiWindowFlags menu_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
                                            ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoNavFocus;

    ImVec2 frame_padding = ImGui::GetStyle().FramePadding;
    draw_window_title();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {frame_padding.x, 4.0f});

    if (ImGui::BeginViewportSideBar("##PrimaryMenuBar", viewport, ImGuiDir_Up, frame_height, menu_flags)) {
      if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
          if (ImGui::MenuItem("New Scene", "Ctrl + N")) {
            new_scene();
          }
          if (ImGui::MenuItem("Open Scene", "Ctrl + O")) {
            open_scene_file_dialog();
          }
          if (ImGui::MenuItem("Save Scene", "Ctrl + S")) {
            save_scene();
          }
          if (ImGui::MenuItem("Save Scene As...", "Ctrl + Shift + S")) {
            save_scene_as();
          }

          ImGui::Separator();
          if (ImGui::MenuItem("New Project")) {
            new_project();
          }
          if (ImGui::MenuItem("Open Project")) {
            const std::string filepath = FileDialogs::open_file({{"Oxylus Project", "oxproj"}});
            open_project(filepath);
          }
          if (ImGui::MenuItem("Save Project")) {
            const std::string filepath = FileDialogs::save_file({{"Oxylus Project", "oxproj"}}, "New Project");
            save_project(filepath);
          }

          ImGui::Separator();
          if (ImGui::MenuItem("Exit")) {
            Application::get()->close();
          }
          ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
          if (ImGui::MenuItem("Settings")) {
            m_editor_panels["EditorSettings"]->Visible = true;
          }
          ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Window")) {
          if (ImGui::MenuItem("Projects", nullptr)) {
            m_editor_panels["ProjectPanel"]->Visible = true;
          }
          if (ImGui::MenuItem("Add viewport", nullptr)) {
            m_viewport_panels.emplace_back(create_scope<ViewportPanel>())->set_context(m_editor_scene, m_scene_hierarchy_panel);
          }
          if (ImGui::MenuItem("Shaders", nullptr)) {
            m_editor_panels["Shaders"]->Visible = true;
          }
          if (ImGui::MenuItem("Framebuffer Viewer", nullptr)) {
            m_editor_panels["FramebufferViewer"]->Visible = true;
          }
          ImGui::MenuItem("Inspector", nullptr, &m_inspector_panel.Visible);
          ImGui::MenuItem("Scene hierarchy", nullptr, &m_scene_hierarchy_panel.Visible);
          ImGui::MenuItem("Console window", nullptr, &m_console_panel.runtime_console.visible);
          ImGui::MenuItem("Performance Overlay", nullptr, &m_viewport_panels[0]->performance_overlay_visible);
          ImGui::MenuItem("Statistics", nullptr, &m_editor_panels["StatisticsPanel"]->Visible);
          ImGui::MenuItem("Editor Debug", nullptr, &m_editor_panels["EditorDebugPanel"]->Visible);
          if (ImGui::BeginMenu("Layout")) {
            if (ImGui::MenuItem("Classic")) {
              set_docking_layout(EditorLayout::Classic);
            }
            if (ImGui::MenuItem("Big Viewport")) {
              set_docking_layout(EditorLayout::BigViewport);
            }
            ImGui::EndMenu();
          }
          ImGui::EndMenu();
        }
        static bool render_asset_manager = false;

        if (ImGui::BeginMenu("Assets")) {
          if (ImGui::MenuItem("Asset Manager")) {
            render_asset_manager = true;
          }
          ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
          if (ImGui::MenuItem("About")) { }
          ImGui::EndMenu();
        }
        ImGui::SameLine();

        //TODO:
#if 0
        if (renderAssetManager) {
          if (ImGui::Begin("Asset Manager")) {
            const auto& assets = AssetManager::GetAssetLibrary();
            if (!assets.MeshAssets.empty()) {
              ImGui::Text("Mesh assets");
              OxUI::BeginProperties();
              for (const auto& [handle, asset] : assets.MeshAssets) {
                auto handleStr = fmt::format("{}", (uint64_t)handle);
                OxUI::Text(handleStr.c_str(), asset.Path.c_str());
              }
              OxUI::EndProperties();
            }
            if (!assets.ImageAssets.empty()) {
              ImGui::Text("Image assets");
              OxUI::BeginProperties();
              for (const auto& asset : assets.ImageAssets) {
                OxUI::Text("Texture:", asset->GetPath().c_str());
              }
              OxUI::EndProperties();
            }
            if (!assets.MaterialAssets.empty()) {
              ImGui::Text("Material assets");
              OxUI::BeginProperties();
              for (const auto& [handle, asset] : assets.MaterialAssets) {
                auto handleStr = fmt::format("{}", (uint64_t)handle);
                OxUI::Text(handleStr.c_str(), asset.Path.c_str());
              }
              OxUI::EndProperties();
            }
            if (ImGui::Button("Package assets")) {
              AssetManager::PackageAssets();
            }
          }
        }
#endif
        {
          //Project name text
          ImGui::SetCursorPos(ImVec2(
            (float)Window::get_width() - 10 -
            ImGui::CalcTextSize(Project::get_active()->get_config().name.c_str()).x,
            0));
          ImGuiScoped::StyleColor b_color1(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 0.7f));
          ImGuiScoped::StyleColor b_color2(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.7f));
          ImGui::Button(Project::get_active()->get_config().name.c_str());
        }

        ImGui::EndMenuBar();
      }
      ImGui::End();
    }
    ImGui::PopStyleVar();

    draw_panels();

    static bool dock_layout_initalized = false;
    if (!dock_layout_initalized) {
      set_docking_layout(current_layout);
      dock_layout_initalized = true;
    }

    ImGui::End();
  }
}

void EditorLayer::editor_shortcuts() {
  if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
    if (ImGui::IsKeyPressed(ImGuiKey_N, false)) {
      new_scene();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_S, false)) {
      save_scene();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_O, false)) {
      open_scene_file_dialog();
    }
    if (ImGui::IsKeyDown(ImGuiKey_LeftShift) && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
      save_scene_as();
    }
  }
}

void EditorLayer::new_scene() {
  const Ref<Scene> new_scene = create_ref<Scene>();
  m_editor_scene = new_scene;
  set_editor_context(new_scene);
  m_last_save_scene_path.clear();
}

void EditorLayer::open_scene_file_dialog() {
  const std::string filepath = FileDialogs::open_file({{"Oxylus Scene", "oxscene"}});
  if (!filepath.empty())
    open_scene(filepath);
}

bool EditorLayer::open_scene(const std::filesystem::path& path) {
  if (!exists(path)) {
    OX_CORE_WARN("Could not find {0}", path.filename().string());
    return false;
  }
  if (path.extension().string() != ".oxscene") {
    OX_CORE_WARN("Could not load {0} - not a scene file", path.filename().string());
    return false;
  }
  const Ref<Scene> new_scene = create_ref<Scene>();
  const SceneSerializer serializer(new_scene);
  if (serializer.deserialize(path.string())) {
    m_editor_scene = new_scene;
    set_editor_context(new_scene);
  }
  m_last_save_scene_path = path.string();
  return true;
}

void EditorLayer::clear_selected_entity() {
  m_scene_hierarchy_panel.clear_selection_context();
}

void EditorLayer::save_scene() {
  if (!m_last_save_scene_path.empty()) {
    ThreadManager::get()->asset_thread.queue_job([this] {
      SceneSerializer(get_active_scene()).serialize(m_last_save_scene_path);
    });
  }
  else {
    save_scene_as();
  }
}

void EditorLayer::save_scene_as() {
  const std::string filepath = FileDialogs::save_file({{"Oxylus Scene", "oxscene"}}, "New Scene");
  if (!filepath.empty()) {
    ThreadManager::get()->asset_thread.queue_job([this, filepath] {
      SceneSerializer(m_editor_scene).serialize(filepath);
    });
    m_last_save_scene_path = filepath;
  }
}

void EditorLayer::on_scene_play() {
  reset_context();
  set_scene_state(SceneState::Play);
  m_active_scene = Scene::copy(m_editor_scene);
  set_editor_context(m_active_scene);
  m_active_scene->on_runtime_start();
}

void EditorLayer::on_scene_stop() {
  reset_context();
  set_scene_state(SceneState::Edit);
  m_active_scene->on_runtime_stop();
  m_active_scene = nullptr;
  set_editor_context(m_editor_scene);
}

void EditorLayer::on_scene_simulate() {
  reset_context();
  set_scene_state(SceneState::Simulate);
  m_active_scene = Scene::copy(m_editor_scene);
  set_editor_context(m_active_scene);
}

void EditorLayer::draw_window_title() {
  if (!Application::get()->get_specification().custom_window_title)
    return;

  ImGuiScoped::StyleColor col(ImGuiCol_MenuBarBg, {0, 0, 0, 1});
  ImGuiScoped::StyleColor col2(ImGuiCol_Button, {0, 0, 0, 0});
  ImGuiScoped::StyleVar var(ImGuiStyleVar_FrameRounding, 0.0f);
  ImGuiScoped::StyleVar var2(ImGuiStyleVar_FrameBorderSize, 0.0f);
  ImGuiScoped::StyleVar var3(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
  ImGuiScoped::StyleVar var4(ImGuiStyleVar_FramePadding, {ImGui::GetWindowSize().x, 7});

  ImGuiScoped::MainMenuBar menubar;

  {
    ImGuiScoped::StyleVar st(ImGuiStyleVar_FramePadding, {0, 8});
    OxUI::image(Resources::editor_resources.engine_icon->get_texture(), {30, 30});
    auto& name = Application::get()->get_specification().name;
    ImGui::Text("%s", name.c_str());
  }

  ImGui::SameLine();

  const ImVec2 region = ImGui::GetContentRegionMax();
  const ImVec2 button_size = {region.y * 1.6f, region.y};

  const float button_start_region = region.x - 3.0f * button_size.x + ImGui::GetStyle().WindowPadding.x;
  // Minimize/Maximize/Close buttons
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0.0f, 0.0f});
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0.0f, 0.0f});
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
  ImGui::PushStyleColor(ImGuiCol_Button, {0.0f, 0.0f, 0.0f, 0.0f});

  ImGui::SetCursorPosX(button_start_region);
  const bool is_normal_cursor = ImGui::GetMouseCursor() == ImGuiMouseCursor_Arrow;

  // Minimize Button
  if (ImGui::Button(StringUtils::from_char8_t(ICON_MDI_MINUS), button_size) && is_normal_cursor)
    Window::minimize();
  ImGui::SameLine();

  // Maximize Button
  if (ImGui::Button(StringUtils::from_char8_t(ICON_MDI_WINDOW_MAXIMIZE), button_size) && is_normal_cursor) {
    if (Window::is_maximized())
      Window::restore();
    else
      Window::maximize();
  }
  ImGui::SameLine();

  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.909f, 0.066f, 0.137f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0.920f, 0.066f, 0.120f, 1.0f});
  // Close Button
  if (ImGui::Button(StringUtils::from_char8_t(ICON_MDI_WINDOW_CLOSE), button_size) && is_normal_cursor)
    Application::get()->close();
  ImGui::PopStyleColor(2);

  ImGui::PopStyleColor();
  ImGui::PopStyleVar(4);
}

void EditorLayer::draw_panels() {
  static EditorPanel* fullscreen_viewport_panel = nullptr;

  for (const auto& panel : m_viewport_panels) {
    if (panel->Visible && !panel->fullscreen_viewport) {
      panel->on_imgui_render();
      if (panel->fullscreen_viewport) {
        fullscreen_viewport_panel = panel.get();
        break;
      }
      fullscreen_viewport_panel = nullptr;
    }
  }

  if (fullscreen_viewport_panel) {
    fullscreen_viewport_panel->on_imgui_render();
    return;
  }

  for (const auto& [name, panel] : m_editor_panels) {
    if (panel->Visible)
      panel->on_imgui_render();
  }

  m_console_panel.on_imgui_render();
  if (m_scene_hierarchy_panel.Visible)
    m_scene_hierarchy_panel.on_imgui_render();
  if (m_inspector_panel.Visible)
    m_inspector_panel.on_imgui_render();
  if (m_content_panel.Visible)
    m_content_panel.on_imgui_render();
  if (m_asset_inspector_panel.Visible)
    m_asset_inspector_panel.on_imgui_render();
}

Ref<Scene> EditorLayer::get_active_scene() {
  return m_active_scene;
}

void EditorLayer::set_editor_context(const Ref<Scene>& scene) {
  m_scene_hierarchy_panel.clear_selection_context();
  m_scene_hierarchy_panel.set_context(scene);
  for (const auto& panel : m_viewport_panels) {
    panel->set_context(scene, m_scene_hierarchy_panel);
  }
}

void EditorLayer::set_scene_state(const SceneState state) {
  scene_state = state;
}

void EditorLayer::set_docking_layout(EditorLayout layout) {
  current_layout = layout;
  ImGui::DockBuilderRemoveNode(dockspace_id);
  ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_PassthruCentralNode);

  const ImVec2 size = ImGui::GetMainViewport()->WorkSize;
  ImGui::DockBuilderSetNodeSize(dockspace_id, size);

  if (layout == EditorLayout::BigViewport) {
    const ImGuiID right_dock = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.8f, nullptr, &dockspace_id);
    ImGuiID left_dock = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.2f, nullptr, &dockspace_id);
    const ImGuiID left_split_dock = ImGui::DockBuilderSplitNode(left_dock, ImGuiDir_Down, 0.4f, nullptr, &left_dock);

    ImGui::DockBuilderDockWindow(m_viewport_panels[0]->get_id(), right_dock);
    ImGui::DockBuilderDockWindow(m_scene_hierarchy_panel.get_id(), left_dock);
    ImGui::DockBuilderDockWindow(m_editor_panels["RenderSettings"]->get_id(), left_split_dock);
    ImGui::DockBuilderDockWindow(m_content_panel.get_id(), left_split_dock);
    ImGui::DockBuilderDockWindow(m_console_panel.runtime_console.id.c_str(), left_split_dock);
    ImGui::DockBuilderDockWindow(m_inspector_panel.get_id(), left_dock);
  }
  else if (layout == EditorLayout::Classic) {
    const ImGuiID right_dock = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.2f, nullptr, &dockspace_id);
    ImGuiID left_dock = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.2f, nullptr, &dockspace_id);
    ImGuiID left_split_vertical_dock = ImGui::DockBuilderSplitNode(left_dock, ImGuiDir_Right, 0.8f, nullptr, &left_dock);
    const ImGuiID bottom_dock = ImGui::DockBuilderSplitNode(left_split_vertical_dock, ImGuiDir_Down, 0.3f, nullptr, &left_split_vertical_dock);
    const ImGuiID left_split_dock = ImGui::DockBuilderSplitNode(left_dock, ImGuiDir_Down, 0.4f, nullptr, &left_dock);

    ImGui::DockBuilderDockWindow(m_scene_hierarchy_panel.get_id(), left_dock);
    ImGui::DockBuilderDockWindow(m_editor_panels["RenderSettings"]->get_id(), left_split_dock);
    ImGui::DockBuilderDockWindow(m_content_panel.get_id(), bottom_dock);
    ImGui::DockBuilderDockWindow(m_console_panel.runtime_console.id.c_str(), bottom_dock);
    ImGui::DockBuilderDockWindow(m_inspector_panel.get_id(), right_dock);
    ImGui::DockBuilderDockWindow(m_viewport_panels[0]->get_id(), left_split_vertical_dock);
  }

  ImGui::DockBuilderFinish(dockspace_id);
}

void EditorLayer::new_project() {
  Project::create_new();
}

void EditorLayer::open_project(const std::filesystem::path& path) {
  if (path.empty())
    return;
  if (Project::load(path)) {
    const auto& start_scene = AssetManager::get_asset_file_system_path(Project::get_active()->get_config().start_scene);
    open_scene(start_scene);
  }
}

void EditorLayer::save_project(const std::string& path) {
  Project::save_active(path);
}
}
