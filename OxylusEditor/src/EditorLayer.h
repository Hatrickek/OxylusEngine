#pragma once

#include <Assets/Assets.h>

#include "OxylusEngine.h"
#include "imgui.h"

#include "Panels/AssetInspectorPanel.h"
#include "Panels/AssetsPanel.h"
#include "Panels/ConsolePanel.h"
#include "Panels/InspectorPanel.h"
#include "Panels/SceneHierarchyPanel.h"
#include "Panels/ViewportPanel.h"
#include "Utils/EditorConfig.h"

namespace Oxylus {
  class EditorLayer : public Layer {
  public:
    EditorLayer();
    ~EditorLayer() override = default;
    void OnAttach(EventDispatcher& dispatcher) override;
    void OnDetach() override;

    void OnUpdate(float deltaTime) override;
    void OnImGuiRender() override;

    static EditorLayer* Get() { return s_Instance; }

    void EditorShortcuts();
    Ref<Scene> GetActiveScene();
    void SetEditorScene(const Ref<Scene>& scene);
    void SetRuntimeScene(const Ref<Scene>& scene);

    enum class SceneState {
      Edit = 0,
      Play = 1,
      Simulate = 2
    };

    void SetSceneState(SceneState state);
    SceneState m_SceneState = SceneState::Edit;

    bool OpenScene(const std::filesystem::path& path);

    void SetSelectedEntity(const Entity& entity);
    Entity GetSelectedEntity() const;
    void ClearSelectedEntity();

    EditorAsset GetSelectedAsset() const;
  private:
    //Project
    void NewProject();
    void OpenProject(const std::filesystem::path& path);
    void SaveProject(const std::string& path);

    //Scene
    void NewScene();
    void OpenScene();
    void SaveScene();
    void SaveSceneAs();
    void OnScenePlay();
    void OnSceneStop();
    void OnSceneSimulate();
    std::string m_LastSaveScenePath{};

    //Panels
    static void DrawWindowTitle();
    void DrawPanels();

  private:
    std::unordered_map<std::string, Scope<EditorPanel>> m_EditorPanels;
    std::vector<Scope<ViewportPanel>> m_ViewportPanels;
    ConsolePanel m_ConsolePanel;
    bool m_ShowStyleEditor = false;
    bool m_ShowDemoWindow = false;
    SceneHierarchyPanel m_SceneHierarchyPanel;
    InspectorPanel m_InspectorPanel;
    AssetsPanel m_AssetsPanel;
    AssetInspectorPanel m_AssetInspectorPanel;

    //Config
    EditorConfig m_EditorConfig;

    Ref<Scene> m_EditorScene;
    Ref<Scene> m_ActiveScene;
    static EditorLayer* s_Instance;
  };
}