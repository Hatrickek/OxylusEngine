#pragma once

#include "EditorPanel.h"
#include "Core/Entity.h"

namespace Oxylus {
  class InspectorPanel : public EditorPanel {
  public:
    InspectorPanel();

    void OnImGuiRender() override;

    static bool DrawMaterialProperties(Ref<Material>& material, bool saveToCurrentPath = false);

  private:
    void DrawComponents(Entity entity) const;

    template <typename Component>
    void DrawAddComponent(Entity entity, const char* name) const;

    void PP_ProbeProperty(bool value) const;

    Entity m_SelectedEntity;
    Ref<Scene> m_Scene;
  };
}
