#pragma once

#include "EditorPanel.h"

namespace Oxylus {
  class ShadersPanel : public EditorPanel {
  public:
    ShadersPanel();
    ~ShadersPanel() override = default;
    void OnImGuiRender() override;
  };
}
