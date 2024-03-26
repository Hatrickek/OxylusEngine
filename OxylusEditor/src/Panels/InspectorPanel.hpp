#pragma once

#include "Assets/Material.hpp"
#include "Scene/Entity.hpp"
#include "EditorPanel.hpp"

namespace ox {
class InspectorPanel : public EditorPanel {
public:
  InspectorPanel();

  void on_imgui_render() override;

  static void draw_material_properties(Shared<Material>& material);

private:
  void draw_components(Entity entity);

  template <typename Component>
  static void draw_add_component(entt::registry& reg, Entity entity, const char* name);

  Entity selected_entity = entt::null;
  Scene* context;
  bool debug_mode = false;
};
}
