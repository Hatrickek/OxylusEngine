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
  enum class SceneState {
    Edit     = 0,
    Play     = 1,
    Simulate = 2
  };

  enum class EditorLayout {
    Classic = 0,
    BigViewport
  };

  SceneState scene_state = SceneState::Edit;

  // Cursors
  GLFWcursor* crosshair_cursor = nullptr;

  // Logo
  Ref<TextureAsset> engine_banner = nullptr;

  // Layout
  ImGuiID dockspace_id;
  EditorLayout current_layout = EditorLayout::Classic;

  EditorLayer();
  ~EditorLayer() override = default;
  void on_attach(EventDispatcher& dispatcher) override;
  void on_detach() override;

  void on_update(const Timestep& delta_time) override;
  void on_imgui_render() override;

  void new_scene();
  void open_scene_file_dialog();
  void save_scene();
  void save_scene_as();
  void on_scene_play();
  void on_scene_stop();
  void on_scene_simulate();

  static EditorLayer* get() { return s_instance; }

  void set_context(EditorContextType type, const char* data, size_t size) { m_editor_context.set(type, data, size); }
  void set_context_as_asset_with_path(const std::string& path) { m_editor_context.set(EditorContextType::Asset, path.c_str(), sizeof(char) * (path.length() + 1)); }
  void set_context_as_file_with_path(const std::string& path) { m_editor_context.set(EditorContextType::File, path.c_str(), sizeof(char) * (path.length() + 1)); }

  void reset_context() { m_editor_context.reset(); }
  const EditorContext& get_context() const { return m_editor_context; }

  void editor_shortcuts();
  Ref<Scene> get_active_scene();
  void set_editor_context(const Ref<Scene>& scene);
  bool open_scene(const std::filesystem::path& path);
  static void load_default_scene(const std::shared_ptr<Scene>& scene);

  Entity get_selected_entity() const { return m_scene_hierarchy_panel.get_selected_entity(); }
  Ref<Scene> get_selected_scene() const { return m_scene_hierarchy_panel.get_scene(); }
  void clear_selected_entity();

  void set_scene_state(SceneState state);
  void set_docking_layout(EditorLayout layout);

private:
  // Project
  static void new_project();
  void open_project(const std::filesystem::path& path);
  static void save_project(const std::string& path);

  // Scene
  std::string m_last_save_scene_path{};

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
  EditorContext m_editor_context = {};

  Ref<Scene> m_editor_scene;
  Ref<Scene> m_active_scene;
  static EditorLayer* s_instance;
};
}
