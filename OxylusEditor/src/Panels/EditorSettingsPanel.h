#pragma once

#include "EditorPanel.h"

namespace ox {
  class EditorSettingsPanel : public EditorPanel {
  public:
    EditorSettingsPanel();
    ~EditorSettingsPanel() override = default;
    void on_imgui_render() override;
  };
}
