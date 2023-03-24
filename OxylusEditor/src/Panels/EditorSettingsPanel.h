#pragma once
#include "EditorPanel.h"

namespace Oxylus {
  class EditorSettingsPanel : public EditorPanel {
  public:
    EditorSettingsPanel();
    ~EditorSettingsPanel() override = default;
    void OnImGuiRender() override;
  };
}
