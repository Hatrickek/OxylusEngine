#pragma once
#include "EditorPanel.h"

namespace Oxylus {
  class EditorDebugPanel : public EditorPanel {
  public:
    EditorDebugPanel();
    void OnImGuiRender() override;
  };
}
