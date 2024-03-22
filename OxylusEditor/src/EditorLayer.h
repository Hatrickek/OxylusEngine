#pragma once

#include "Core/Layer.h"
#include "EditorContext.h"

#include "Panels/ContentPanel.h"
#include "Panels/SceneHierarchyPanel.h"
#include "Panels/ViewportPanel.h"

#include "Render/Window.h"
#include "Utils/EditorConfig.h"

#include "UI/RuntimeConsole.h"
#include "Utils/Archive.h"

namespace ox {
enum class HistoryOp : uint32_t {
  Translator,    // translator interaction
  Selection,     // selection changed
  Add,           // entity added
  Delete,        // entity removed
  ComponentData, // generic component data changed
  None
};

class EditorLayer : public Layer {
public:
  enum class SceneState { Edit = 0, Play = 1, Simulate = 2 };

  enum class EditorLayout { Classic = 0, BigViewport };

  SceneState scene_state = SceneState::Edit;

  // Panels
  ankerl::unordered_dense::map<size_t, Unique<EditorPanel>> editor_panels;
  std::vector<Unique<ViewportPanel>> viewport_panels;

  template <typename T> void add_panel() { editor_panels.emplace(typeid(T).hash_code(), create_unique<T>()); }

  template <typename T> T* get_panel() {
    const auto hash_code = typeid(T).hash_code();
    OX_CORE_ASSERT(editor_panels.contains(hash_code));
    return dynamic_cast<T*>(editor_panels[hash_code].get());
  }

  // Logo
  Shared<TextureAsset> engine_banner = nullptr;

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

  static EditorLayer* get() { return instance; }

  void set_context(EditorContextType type, const char* data, size_t size) { editor_context.set(type, data, size); }
  void set_context_as_asset_with_path(const std::string& path) {
    editor_context.set(EditorContextType::Asset, path.c_str(), sizeof(char) * (path.length() + 1));
  }
  void set_context_as_file_with_path(const std::string& path) {
    editor_context.set(EditorContextType::File, path.c_str(), sizeof(char) * (path.length() + 1));
  }

  void reset_context() { editor_context.reset(); }
  const EditorContext& get_context() const { return editor_context; }

  void editor_shortcuts();
  Shared<Scene> get_active_scene();
  void set_editor_context(const Shared<Scene>& scene);
  bool open_scene(const std::filesystem::path& path);
  static void load_default_scene(const std::shared_ptr<Scene>& scene);

  Entity get_selected_entity() { return get_panel<SceneHierarchyPanel>()->get_selected_entity(); }
  Shared<Scene> get_selected_scene() { return get_panel<SceneHierarchyPanel>()->get_scene(); }
  void clear_selected_entity();

  void set_scene_state(SceneState state);
  void set_docking_layout(EditorLayout layout);

  Archive& advance_history();

private:
  // Project
  static void new_project();
  static void save_project(const std::string& path);

  // Scene
  std::string last_save_scene_path{};

  void draw_panels();
  RuntimeConsole runtime_console = {};

  // Config
  EditorConfig editor_config;

  // Context
  EditorContext editor_context = {};
  std::vector<Archive> history;
  int historyPos = -1;

  Shared<Scene> editor_scene;
  Shared<Scene> active_scene;
  static EditorLayer* instance;
};
} // namespace ox
