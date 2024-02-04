#pragma once
#include "EditorPanel.h"

namespace Ox {
  class EditorDebugPanel : public EditorPanel {
  public:
    EditorDebugPanel();
    void on_imgui_render() override;
  };
}
