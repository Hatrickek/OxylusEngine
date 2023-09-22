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
#include "Render/Vulkan/VulkanRenderer.h"
#include "UI/IGUI.h"
#include "Utils/EditorConfig.h"
#include "Utils/ImGuiScoped.h"
#include "Utils/UIUtils.h"

#include "Scene/SceneSerializer.h"
#include "Utils/EmbeddedBanner.h"
#include "Utils/StringUtils.h"

namespace Oxylus {
EditorLayer* EditorLayer::s_Instance = nullptr;

EditorLayer::EditorLayer() : Layer("Editor Layer") {
  s_Instance = this;
}

void EditorLayer::OnAttach(EventDispatcher& dispatcher) {
  m_EditorConfig.LoadConfig();
  Resources::InitEditorResources();

  m_CrosshairCursor = Input::LoadCursorIconStandard(GLFW_CROSSHAIR_CURSOR);

  m_EngineBanner = CreateRef<TextureAsset>();
  m_EngineBanner->CreateImage(EngineBannerWidth, EngineBannerHeight, EngineBanner);

  Input::SetCursorState(Input::CursorState::NORMAL, Window::GetGLFWWindow());

  m_EditorScene = CreateRef<Scene>();

  // Initialize panels
  m_EditorPanels.emplace("EditorSettings", CreateScope<EditorSettingsPanel>());
  m_EditorPanels.emplace("RenderSettings", CreateScope<RendererSettingsPanel>());
  m_EditorPanels.emplace("Shaders", CreateScope<ShadersPanel>());
  m_EditorPanels.emplace("FramebufferViewer", CreateScope<FramebufferViewerPanel>());
  m_EditorPanels.emplace("ProjectPanel", CreateScope<ProjectPanel>());
  m_EditorPanels.emplace("StatisticsPanel", CreateScope<StatisticsPanel>());
  m_EditorPanels.emplace("EditorDebugPanel", CreateScope<EditorDebugPanel>());
  m_ViewportPanels.emplace_back(CreateScope<ViewportPanel>())->m_Camera.SetPosition({-2, 2, 0});

  // Register panel events
  m_ConsolePanel.m_RuntimeConsole.RegisterCommand("show_style_editor",
    "show_style_editor",
    [this] {
      m_ShowStyleEditor = !m_ShowStyleEditor;
    });
  m_ConsolePanel.m_RuntimeConsole.RegisterCommand("show_imgui_demo",
    "show_imgui_demo",
    [this] {
      m_ShowDemoWindow = !m_ShowDemoWindow;
    });
  SetEditorScene(m_EditorScene);
}

void EditorLayer::OnDetach() {
  Input::DestroyCursor(m_CrosshairCursor);
  m_EditorConfig.SaveConfig();
}

void EditorLayer::OnUpdate(Timestep deltaTime) {
  for (const auto& [name, panel] : m_EditorPanels) {
    if (!panel->Visible)
      continue;
    panel->OnUpdate();
  }
  for (const auto& panel : m_ViewportPanels) {
    if (!panel->Visible)
      continue;
    panel->OnUpdate();
  }
  if (m_SceneHierarchyPanel.Visible)
    m_SceneHierarchyPanel.OnUpdate();
  if (m_ContentPanel.Visible)
    m_ContentPanel.OnUpdate();
  if (m_InspectorPanel.Visible)
    m_InspectorPanel.OnUpdate();

  m_AssetInspectorPanel.OnUpdate();

  switch (m_SceneState) {
    case SceneState::Edit: {
      m_ActiveScene->OnEditorUpdate(deltaTime, m_ViewportPanels[0]->m_Camera);
      break;
    }
    case SceneState::Play: {
      m_ActiveScene->OnRuntimeUpdate(deltaTime);
      break;
    }
    case SceneState::Simulate: {
      m_ActiveScene->OnEditorUpdate(deltaTime, m_ViewportPanels[0]->m_Camera);
      break;
    }
  }
}

void EditorLayer::OnImGuiRender() {
  if (m_ShowStyleEditor)
    ImGui::ShowStyleEditor();
  if (m_ShowDemoWindow)
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
            Application::Get()->Close();
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
            m_ViewportPanels.emplace_back(CreateScope<ViewportPanel>())->SetContext(m_ActiveScene,
              m_SceneHierarchyPanel);
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
          ImGui::MenuItem("Performance Overlay", nullptr, &m_ViewportPanels[0]->PerformanceOverlayVisible);
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
              IGUI::BeginProperties();
              for (const auto& [handle, asset] : assets.MeshAssets) {
                auto handleStr = fmt::format("{}", (uint64_t)handle);
                IGUI::Text(handleStr.c_str(), asset.Path.c_str());
              }
              IGUI::EndProperties();
            }
            if (!assets.ImageAssets.empty()) {
              ImGui::Text("Image assets");
              IGUI::BeginProperties();
              for (const auto& asset : assets.ImageAssets) {
                IGUI::Text("Texture:", asset->GetPath().c_str());
              }
              IGUI::EndProperties();
            }
            if (!assets.MaterialAssets.empty()) {
              ImGui::Text("Material assets");
              IGUI::BeginProperties();
              for (const auto& [handle, asset] : assets.MaterialAssets) {
                auto handleStr = fmt::format("{}", (uint64_t)handle);
                IGUI::Text(handleStr.c_str(), asset.Path.c_str());
              }
              IGUI::EndProperties();
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
            (float)Window::GetWidth() - 10 -
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
  const Ref<Scene> newScene = CreateRef<Scene>();
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
  const Ref<Scene> newScene = CreateRef<Scene>();
  const SceneSerializer serializer(newScene);
  if (serializer.Deserialize(path.string())) {
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
    ThreadManager::Get()->AssetThread.QueueJob([this] {
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
    ThreadManager::Get()->AssetThread.QueueJob([this, filepath] {
      SceneSerializer(GetActiveScene()).Serialize(filepath);
    });
    m_LastSaveScenePath = filepath;
  }
}

void EditorLayer::OnScenePlay() {
  ResetContext();
  m_ActiveScene = Scene::Copy(m_EditorScene);
  SetRuntimeScene(m_ActiveScene);

  m_ActiveScene->OnRuntimeStart();
  SetSceneState(SceneState::Play);
}

void EditorLayer::OnSceneStop() {
  ResetContext();
  SetEditorScene(m_EditorScene);

  m_ActiveScene->OnRuntimeStop();
  SetSceneState(SceneState::Edit);
}

void EditorLayer::OnSceneSimulate() {
  SetSceneState(SceneState::Simulate);
  m_ActiveScene = Scene::Copy(m_EditorScene);
  SetRuntimeScene(m_ActiveScene);
}

void EditorLayer::DrawWindowTitle() {
  if (!Application::Get()->GetSpecification().CustomWindowTitle)
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
    IGUI::Image(Resources::s_EditorResources.EngineIcon->GetTexture(), {30, 30});
    auto& name = Application::Get()->GetSpecification().Name;
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
  if (ImGui::Button(StringUtils::FromChar8T(ICON_MDI_MINUS), buttonSize) && isNormalCursor)
    Window::Minimize();
  ImGui::SameLine();

  // Maximize Button
  if (ImGui::Button(StringUtils::FromChar8T(ICON_MDI_WINDOW_MAXIMIZE), buttonSize) && isNormalCursor) {
    if (Window::IsMaximized())
      Window::Restore();
    else
      Window::Maximize();
  }
  ImGui::SameLine();

  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.909f, 0.066f, 0.137f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0.920f, 0.066f, 0.120f, 1.0f});
  // Close Button
  if (ImGui::Button(StringUtils::FromChar8T(ICON_MDI_WINDOW_CLOSE), buttonSize) && isNormalCursor)
    Application::Get()->Close();
  ImGui::PopStyleColor(2);

  ImGui::PopStyleColor();
  ImGui::PopStyleVar(4);
}

void EditorLayer::DrawPanels() {
  static EditorPanel* fullscreenViewportPanel = nullptr;

  for (const auto& panel : m_ViewportPanels) {
    if (panel->Visible && !panel->FullscreenViewport) {
      panel->OnImGuiRender();
      if (panel->FullscreenViewport) {
        fullscreenViewportPanel = panel.get();
        break;
      }
      fullscreenViewportPanel = nullptr;
    }
  }

  if (fullscreenViewportPanel) {
    fullscreenViewportPanel->OnImGuiRender();
    return;
  }

  for (const auto& [name, panel] : m_EditorPanels) {
    if (panel->Visible)
      panel->OnImGuiRender();
  }

  m_ConsolePanel.OnImGuiRender();
  if (m_SceneHierarchyPanel.Visible)
    m_SceneHierarchyPanel.OnImGuiRender();
  if (m_InspectorPanel.Visible)
    m_InspectorPanel.OnImGuiRender();
  if (m_ContentPanel.Visible)
    m_ContentPanel.OnImGuiRender();
  if (m_AssetInspectorPanel.Visible)
    m_AssetInspectorPanel.OnImGuiRender();
}

Ref<Scene> EditorLayer::GetActiveScene() {
  return m_ActiveScene;
}

void EditorLayer::SetEditorScene(const Ref<Scene>& scene) {
  m_EditorScene = scene;
  m_ActiveScene = scene;
  m_SceneHierarchyPanel.ClearSelectionContext();
  m_SceneHierarchyPanel.SetContext(scene);
  for (const auto& panel : m_ViewportPanels) {
    panel->SetContext(m_EditorScene, m_SceneHierarchyPanel);
  }
}

void EditorLayer::SetRuntimeScene(const Ref<Scene>& scene) {
  m_ActiveScene = scene;
  m_SceneHierarchyPanel.ClearSelectionContext();
  m_SceneHierarchyPanel.SetContext(scene);
  for (const auto& panel : m_ViewportPanels) {
    panel->SetContext(m_ActiveScene, m_SceneHierarchyPanel);
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
    const auto& startScene = AssetManager::GetAssetFileSystemPath(Project::GetActive()->GetConfig().StartScene);
    OpenScene(startScene);
  }
}

void EditorLayer::SaveProject(const std::string& path) {
  Project::SaveActive(path);
}
}
