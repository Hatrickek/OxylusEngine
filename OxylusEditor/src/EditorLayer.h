#pragma once

#include "EditorContext.h"
#include "Core/Layer.h"

#include "Panels/AssetInspectorPanel.h"
#include "Panels/ContentPanel.h"
#include "Panels/ConsolePanel.h"
#include "Panels/InspectorPanel.h"
#include "Panels/SceneHierarchyPanel.h"
#include "Panels/ViewportPanel.h"
#include "Utils/EditorConfig.h"
#include "Render/Window.h"

namespace Oxylus {
class EditorLayer : public Layer {
public:
  EditorLayer();
  ~EditorLayer() override = default;
  void on_attach(EventDispatcher& dispatcher) override;
  void on_detach() override;

  void on_update(Timestep deltaTime) override;
  void on_imgui_render() override;

  void OnScenePlay();
  void OnSceneStop();
  void OnSceneSimulate();

  static EditorLayer* get() { return s_Instance; }

  void SetContext(EditorContextType type, const char* data, size_t size) { m_SelectedContext.Set(type, data, size); }
  void SetContextAsAssetWithPath(const std::string& path) { m_SelectedContext.Set(EditorContextType::Asset, path.c_str(), sizeof(char) * (path.length() + 1)); }
  void SetContextAsFileWithPath(const std::string& path) { m_SelectedContext.Set(EditorContextType::File, path.c_str(), sizeof(char) * (path.length() + 1)); }

  void ResetContext() { m_SelectedContext.Reset(); }
  const EditorContext& GetContext() const { return m_SelectedContext; }

  void EditorShortcuts();
  Ref<Scene> GetActiveScene();
  void SetEditorScene(const Ref<Scene>& scene);
  void SetRuntimeScene(const Ref<Scene>& scene);
  bool OpenScene(const std::filesystem::path& path);

  void SetSelectedEntity(const Entity& entity);
  Entity get_selected_entity() const { return m_SceneHierarchyPanel.GetSelectedEntity(); }
  Ref<Scene> GetSelectedScene() const { return m_SceneHierarchyPanel.GetScene(); }
  void ClearSelectedEntity();

  enum class SceneState {
    Edit     = 0,
    Play     = 1,
    Simulate = 2
  };

  void SetSceneState(SceneState state);

  SceneState m_SceneState = SceneState::Edit;

  // Cursors
  GLFWcursor* m_CrosshairCursor = nullptr;

  // Logo
  Ref<TextureAsset> m_EngineBanner = nullptr;

private:
  // Project
  void NewProject();
  void OpenProject(const std::filesystem::path& path);
  void SaveProject(const std::string& path);

  // Scene
  void NewScene();
  void OpenScene();
  void SaveScene();
  void SaveSceneAs();
  std::string m_LastSaveScenePath{};

  // Panels
  static void DrawWindowTitle();
  void DrawPanels();
  std::unordered_map<std::string, Scope<EditorPanel>> m_EditorPanels;
  std::vector<Scope<ViewportPanel>> m_ViewportPanels;
  bool m_ShowStyleEditor = false;
  bool m_ShowDemoWindow = false;
  ConsolePanel m_ConsolePanel;
  SceneHierarchyPanel m_SceneHierarchyPanel;
  InspectorPanel m_InspectorPanel;
  ContentPanel m_ContentPanel;
  AssetInspectorPanel m_AssetInspectorPanel;

  // Config
  EditorConfig m_EditorConfig;

  // Context
  EditorContext m_SelectedContext = {};

  Ref<Scene> m_EditorScene;
  Ref<Scene> m_ActiveScene;
  static EditorLayer* s_Instance;
};
}
