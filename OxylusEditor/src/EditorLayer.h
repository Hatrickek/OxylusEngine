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

  void on_scene_play();
  void on_scene_stop();
  void on_scene_simulate();

  static EditorLayer* get() { return s_instance; }

  void set_context(EditorContextType type, const char* data, size_t size) { m_selected_context.Set(type, data, size); }
  void set_context_as_asset_with_path(const std::string& path) { m_selected_context.Set(EditorContextType::Asset, path.c_str(), sizeof(char) * (path.length() + 1)); }
  void set_context_as_file_with_path(const std::string& path) { m_selected_context.Set(EditorContextType::File, path.c_str(), sizeof(char) * (path.length() + 1)); }

  void reset_context() { m_selected_context.Reset(); }
  const EditorContext& get_context() const { return m_selected_context; }

  void editor_shortcuts();
  Ref<Scene> get_active_scene();
  void set_editor_scene(const Ref<Scene>& scene);
  void set_runtime_scene(const Ref<Scene>& scene);
  bool open_scene(const std::filesystem::path& path);

  void set_selected_entity(const Entity& entity);
  Entity get_selected_entity() const { return m_scene_hierarchy_panel.GetSelectedEntity(); }
  Ref<Scene> get_selected_scene() const { return m_scene_hierarchy_panel.GetScene(); }
  void clear_selected_entity();

  enum class SceneState {
    Edit     = 0,
    Play     = 1,
    Simulate = 2
  };

  void set_scene_state(SceneState state);

  SceneState scene_state = SceneState::Edit;

  // Cursors
  GLFWcursor* crosshair_cursor = nullptr;

  // Logo
  Ref<TextureAsset> engine_banner = nullptr;

private:
  // Project
  void new_project();
  void open_project(const std::filesystem::path& path);
  void save_project(const std::string& path);

  // Scene
  void new_scene();
  void open_scene();
  void save_scene();
  void save_scene_as();
  std::string m_LastSaveScenePath{};

  // Panels
  static void draw_window_title();
  void draw_panels();
  std::unordered_map<std::string, Scope<EditorPanel>> m_editor_panels;
  std::vector<Scope<ViewportPanel>> m_viewport_panels;
  ConsolePanel m_console_panel;
  SceneHierarchyPanel m_scene_hierarchy_panel;
  InspectorPanel m_inspector_panel;
  ContentPanel m_content_panel;
  AssetInspectorPanel m_asset_inspector_panel;

  // Config
  EditorConfig m_editor_config;

  // Context
  EditorContext m_selected_context = {};

  Ref<Scene> m_editor_scene;
  Ref<Scene> m_active_scene;
  static EditorLayer* s_instance;
};
}
