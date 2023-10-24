#pragma once
#include "EditorPanel.h"

namespace Oxylus {
  class EditorDebugPanel : public EditorPanel {
  public:
    EditorDebugPanel();
    void on_imgui_render() override;
  };
}
