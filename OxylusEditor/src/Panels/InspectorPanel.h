#pragma once

#include "EditorPanel.h"
#include "Scene/Entity.h"

namespace Ox {
class InspectorPanel : public EditorPanel {
public:
  InspectorPanel();

  void on_imgui_render() override;

  static bool draw_material_properties(Shared<Material>& material, bool save_to_current_path = false);

private:
  void draw_components(Entity entity) const;

  template <typename Component>
  static void draw_add_component(Entity entity, const char* name);

  void pp_probe_property(bool value, const PostProcessProbe& component) const;

  Entity m_SelectedEntity;
  Scene* m_Scene;
};
}
