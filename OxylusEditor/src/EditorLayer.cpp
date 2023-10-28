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
EditorLayer* EditorLayer::s_Instance = nullptr;

AutoCVar_Int cvar_show_style_editor("ui.imgui_style_editor", "show imgui style editor", 0, CVarFlags::EditCheckbox);
AutoCVar_Int cvar_show_imgui_demo("ui.imgui_demo", "show imgui demo window", 0, CVarFlags::EditCheckbox);

EditorLayer::EditorLayer() : Layer("Editor Layer") {
  s_Instance = this;
}

void EditorLayer::on_attach(EventDispatcher& dispatcher) {
  m_EditorConfig.LoadConfig();
  Resources::init_editor_resources();

  m_CrosshairCursor = Input::LoadCursorIconStandard(GLFW_CROSSHAIR_CURSOR);

  m_EngineBanner = create_ref<TextureAsset>();
  m_EngineBanner->create_texture(EngineBannerWidth, EngineBannerHeight, EngineBanner);

  Input::SetCursorState(Input::CursorState::NORMAL, Window::get_glfw_window());

  auto scene = create_ref<Scene>();
  SetEditorScene(scene);

  // Initialize panels
  m_EditorPanels.emplace("EditorSettings", create_scope<EditorSettingsPanel>());
  m_EditorPanels.emplace("RenderSettings", create_scope<RendererSettingsPanel>());
  m_EditorPanels.emplace("Shaders", create_scope<ShadersPanel>());
  m_EditorPanels.emplace("FramebufferViewer", create_scope<FramebufferViewerPanel>());
  m_EditorPanels.emplace("ProjectPanel", create_scope<ProjectPanel>());
  m_EditorPanels.emplace("StatisticsPanel", create_scope<StatisticsPanel>());
  m_EditorPanels.emplace("EditorDebugPanel", create_scope<EditorDebugPanel>());
  const auto& viewport = m_ViewportPanels.emplace_back(create_scope<ViewportPanel>());
  viewport->m_camera.SetPosition({-2, 2, 0});
  viewport->set_context(m_EditorScene, m_SceneHierarchyPanel);
}

void EditorLayer::on_detach() {
  Input::DestroyCursor(m_CrosshairCursor);
  m_EditorConfig.SaveConfig();
}

void EditorLayer::on_update(Timestep deltaTime) {
  for (const auto& [name, panel] : m_EditorPanels) {
    if (!panel->Visible)
      continue;
    panel->on_update();
  }
  for (const auto& panel : m_ViewportPanels) {
    if (!panel->Visible)
      continue;
    panel->on_update();
  }
  if (m_SceneHierarchyPanel.Visible)
    m_SceneHierarchyPanel.on_update();
  if (m_ContentPanel.Visible)
    m_ContentPanel.on_update();
  if (m_InspectorPanel.Visible)
    m_InspectorPanel.on_update();

  m_AssetInspectorPanel.on_update();

  switch (m_SceneState) {
    case SceneState::Edit: {
      m_EditorScene->on_editor_update(deltaTime, m_ViewportPanels[0]->m_camera);
      break;
    }
    case SceneState::Play: {
      m_ActiveScene->on_runtime_update(deltaTime);
      break;
    }
    case SceneState::Simulate: {
      m_ActiveScene->on_editor_update(deltaTime, m_ViewportPanels[0]->m_camera);
      break;
    }
  }
}

void EditorLayer::on_imgui_render() {
  if (cvar_show_style_editor.get())
    ImGui::ShowStyleEditor();
  if (cvar_show_imgui_demo.get())
    ImGui::ShowDemoWindow();

  EditorShortcuts();

  static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;

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
  if (ImGui::Begin("DockSpace Demo", nullptr, window_flags)) {
    ImGui::PopStyleVar(3);

    // Submit the DockSpace
    const ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
      const ImGuiID dockspace_id = ImGui::GetID("Main dockspace");
      ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
    }

    const float frameHeight = ImGui::GetFrameHeight();

    constexpr ImGuiWindowFlags menu_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
                                            ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoNavFocus;

    ImVec2 framePadding = ImGui::GetStyle().FramePadding;
    DrawWindowTitle();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {framePadding.x, 4.0f});

    if (ImGui::BeginViewportSideBar("##PrimaryMenuBar", viewport, ImGuiDir_Up, frameHeight, menu_flags)) {
      if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
          if (ImGui::MenuItem("New Scene", "Ctrl + N")) {
            NewScene();
          }
          if (ImGui::MenuItem("Open Scene", "Ctrl + O")) {
            OpenScene();
          }
          if (ImGui::MenuItem("Save Scene", "Ctrl + S")) {
            SaveScene();
          }
          if (ImGui::MenuItem("Save Scene As...", "Ctrl + Shift + S")) {
            SaveSceneAs();
          }

          ImGui::Separator();
          if (ImGui::MenuItem("New Project")) {
            NewProject();
          }
          if (ImGui::MenuItem("Open Project")) {
            const std::string filepath = FileDialogs::OpenFile({{"Oxylus Project", "oxproj"}});
            OpenProject(filepath);
          }
          if (ImGui::MenuItem("Save Project")) {
            const std::string filepath = FileDialogs::SaveFile({{"Oxylus Project", "oxproj"}}, "New Project");
            SaveProject(filepath);
          }

          ImGui::Separator();
          if (ImGui::MenuItem("Exit")) {
            Application::get()->close();
          }
          ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
          if (ImGui::MenuItem("Settings")) {
            m_EditorPanels["EditorSettings"]->Visible = true;
          }
          ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Window")) {
          if (ImGui::MenuItem("Add viewport", nullptr)) {
            m_ViewportPanels.emplace_back(create_scope<ViewportPanel>())->set_context(m_EditorScene, m_SceneHierarchyPanel);
          }
          if (ImGui::MenuItem("Shaders", nullptr)) {
            m_EditorPanels["Shaders"]->Visible = true;
          }
          if (ImGui::MenuItem("Framebuffer Viewer", nullptr)) {
            m_EditorPanels["FramebufferViewer"]->Visible = true;
          }
          ImGui::MenuItem("Inspector", nullptr, &m_InspectorPanel.Visible);
          ImGui::MenuItem("Scene hierarchy", nullptr, &m_SceneHierarchyPanel.Visible);
          ImGui::MenuItem("Console window", nullptr, &m_ConsolePanel.m_RuntimeConsole.Visible);
          ImGui::MenuItem("Performance Overlay", nullptr, &m_ViewportPanels[0]->performance_overlay_visible);
          ImGui::MenuItem("Statistics", nullptr, &m_EditorPanels["StatisticsPanel"]->Visible);
          ImGui::MenuItem("Editor Debug", nullptr, &m_EditorPanels["EditorDebugPanel"]->Visible);
          ImGui::EndMenu();
        }

        static bool renderAssetManager = false;

        if (ImGui::BeginMenu("Assets")) {
          if (ImGui::MenuItem("Asset Manager")) {
            renderAssetManager = true;
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
            ImGui::CalcTextSize(Project::GetActive()->GetConfig().Name.c_str()).x,
            0));
          ImGuiScoped::StyleColor bColor1(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 0.7f));
          ImGuiScoped::StyleColor bColor2(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 0.7f));
          ImGui::Button(Project::GetActive()->GetConfig().Name.c_str());
        }

        ImGui::EndMenuBar();
      }
      ImGui::End();
    }
    ImGui::PopStyleVar();

    DrawPanels();

    ImGui::End();
  }
}

void EditorLayer::EditorShortcuts() {
  if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
    if (ImGui::IsKeyPressed(ImGuiKey_N, false)) {
      NewScene();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_S, false)) {
      SaveScene();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_O, false)) {
      OpenScene();
    }
    if (ImGui::IsKeyDown(ImGuiKey_LeftShift) && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
      SaveSceneAs();
    }
  }
}

void EditorLayer::NewScene() {
  const Ref<Scene> newScene = create_ref<Scene>();
  SetEditorScene(newScene);
  m_LastSaveScenePath.clear();
}

void EditorLayer::OpenScene() {
  const std::string filepath = FileDialogs::OpenFile({{"Oxylus Scene", "oxscene"}});
  if (!filepath.empty())
    OpenScene(filepath);
}

bool EditorLayer::OpenScene(const std::filesystem::path& path) {
  if (!exists(path)) {
    OX_CORE_WARN("Could not find {0}", path.filename().string());
    return false;
  }
  if (path.extension().string() != ".oxscene") {
    OX_CORE_WARN("Could not load {0} - not a scene file", path.filename().string());
    return false;
  }
  const Ref<Scene> newScene = create_ref<Scene>();
  const SceneSerializer serializer(newScene);
  if (serializer.deserialize(path.string())) {
    SetEditorScene(newScene);
  }
  m_LastSaveScenePath = path.string();
  return true;
}

void EditorLayer::SetSelectedEntity(const Entity& entity) {
  m_SceneHierarchyPanel.SetSelectedEntity(entity);
}

void EditorLayer::ClearSelectedEntity() {
  m_SceneHierarchyPanel.ClearSelectionContext();
}

void EditorLayer::SaveScene() {
  if (!m_LastSaveScenePath.empty()) {
    ThreadManager::get()->asset_thread.queue_job([this] {
      SceneSerializer(GetActiveScene()).Serialize(m_LastSaveScenePath);
    });
  }
  else {
    SaveSceneAs();
  }
}

void EditorLayer::SaveSceneAs() {
  const std::string filepath = FileDialogs::SaveFile({{"Oxylus Scene", "oxscene"}}, "New Scene");
  if (!filepath.empty()) {
    ThreadManager::get()->asset_thread.queue_job([this, filepath] {
      SceneSerializer(GetActiveScene()).Serialize(filepath);
    });
    m_LastSaveScenePath = filepath;
  }
}

void EditorLayer::OnScenePlay() {
  ResetContext();
  m_ActiveScene = Scene::copy(m_EditorScene);
  SetRuntimeScene(m_ActiveScene);

  m_ActiveScene->on_runtime_start();
  SetSceneState(SceneState::Play);
}

void EditorLayer::OnSceneStop() {
  ResetContext();
  SetEditorScene(m_EditorScene);

  m_ActiveScene->on_runtime_stop();
  SetSceneState(SceneState::Edit);
}

void EditorLayer::OnSceneSimulate() {
  SetSceneState(SceneState::Simulate);
  m_ActiveScene = Scene::copy(m_EditorScene);
  SetRuntimeScene(m_ActiveScene);
}

void EditorLayer::DrawWindowTitle() {
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
  const ImVec2 buttonSize = {region.y * 1.6f, region.y};

  const float buttonStartRegion = region.x - 3.0f * buttonSize.x + ImGui::GetStyle().WindowPadding.x;
  // Minimize/Maximize/Close buttons
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0.0f, 0.0f});
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0.0f, 0.0f});
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
  ImGui::PushStyleColor(ImGuiCol_Button, {0.0f, 0.0f, 0.0f, 0.0f});

  ImGui::SetCursorPosX(buttonStartRegion);
  const bool isNormalCursor = ImGui::GetMouseCursor() == ImGuiMouseCursor_Arrow;

  // Minimize Button
  if (ImGui::Button(StringUtils::from_char8_t(ICON_MDI_MINUS), buttonSize) && isNormalCursor)
    Window::minimize();
  ImGui::SameLine();

  // Maximize Button
  if (ImGui::Button(StringUtils::from_char8_t(ICON_MDI_WINDOW_MAXIMIZE), buttonSize) && isNormalCursor) {
    if (Window::is_maximized())
      Window::restore();
    else
      Window::maximize();
  }
  ImGui::SameLine();

  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.909f, 0.066f, 0.137f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0.920f, 0.066f, 0.120f, 1.0f});
  // Close Button
  if (ImGui::Button(StringUtils::from_char8_t(ICON_MDI_WINDOW_CLOSE), buttonSize) && isNormalCursor)
    Application::get()->close();
  ImGui::PopStyleColor(2);

  ImGui::PopStyleColor();
  ImGui::PopStyleVar(4);
}

void EditorLayer::DrawPanels() {
  static EditorPanel* fullscreenViewportPanel = nullptr;

  for (const auto& panel : m_ViewportPanels) {
    if (panel->Visible && !panel->FullscreenViewport) {
      panel->on_imgui_render();
      if (panel->FullscreenViewport) {
        fullscreenViewportPanel = panel.get();
        break;
      }
      fullscreenViewportPanel = nullptr;
    }
  }

  if (fullscreenViewportPanel) {
    fullscreenViewportPanel->on_imgui_render();
    return;
  }

  for (const auto& [name, panel] : m_EditorPanels) {
    if (panel->Visible)
      panel->on_imgui_render();
  }

  m_ConsolePanel.OnImGuiRender();
  if (m_SceneHierarchyPanel.Visible)
    m_SceneHierarchyPanel.on_imgui_render();
  if (m_InspectorPanel.Visible)
    m_InspectorPanel.on_imgui_render();
  if (m_ContentPanel.Visible)
    m_ContentPanel.on_imgui_render();
  if (m_AssetInspectorPanel.Visible)
    m_AssetInspectorPanel.on_imgui_render();
}

Ref<Scene> EditorLayer::GetActiveScene() {
  return m_ActiveScene;
}

void EditorLayer::SetEditorScene(const Ref<Scene>& scene) {
  m_EditorScene.reset();
  m_ActiveScene.reset();
  m_EditorScene = scene;
  m_ActiveScene = scene;
  m_SceneHierarchyPanel.ClearSelectionContext();
  m_SceneHierarchyPanel.SetContext(scene);
  for (const auto& panel : m_ViewportPanels) {
    panel->set_context(m_EditorScene, m_SceneHierarchyPanel);
  }
}

void EditorLayer::SetRuntimeScene(const Ref<Scene>& scene) {
  m_ActiveScene = scene;
  m_SceneHierarchyPanel.ClearSelectionContext();
  m_SceneHierarchyPanel.SetContext(scene);
  for (const auto& panel : m_ViewportPanels) {
    panel->set_context(m_ActiveScene, m_SceneHierarchyPanel);
  }
}

void EditorLayer::SetSceneState(const SceneState state) {
  m_SceneState = state;
}

void EditorLayer::NewProject() {
  Project::New();
}

void EditorLayer::OpenProject(const std::filesystem::path& path) {
  if (path.empty())
    return;
  if (Project::Load(path)) {
    const auto& startScene = AssetManager::get_asset_file_system_path(Project::GetActive()->GetConfig().StartScene);
    OpenScene(startScene);
  }
}

void EditorLayer::SaveProject(const std::string& path) {
  Project::SaveActive(path);
}
}
