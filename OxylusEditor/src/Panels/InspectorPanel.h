#pragma once
#include "EditorPanel.h"
#include "Core/Entity.h"

namespace Oxylus {
  class InspectorPanel : public EditorPanel {
  public:
    InspectorPanel();

    void OnImGuiRender() override;

  private:
    void DrawComponents(Entity entity);

    template <typename Component>
    void DrawAddComponent(Entity entity, const char* name) const;

    Entity m_SelectedEntity;
  };
}
