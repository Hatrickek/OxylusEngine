#pragma once

#include "EditorPanel.h"
#include "Scene/Entity.h"
#include "Assets/Material.h"

namespace Ox {
class InspectorPanel : public EditorPanel {
public:
  InspectorPanel();

  void on_imgui_render() override;

  static void draw_material_properties(Shared<Material>& material);

private:
  void draw_components(Entity entity) const;

  template <typename Component>
  static void draw_add_component(entt::registry& reg, Entity entity, const char* name);

  Entity selected_entity = entt::null;
  Scene* context;
};
}
