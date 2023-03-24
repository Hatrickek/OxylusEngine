#pragma once
#include <vector>

#include "EditorPanel.h"

namespace Oxylus {
  class RendererSettingsPanel : public EditorPanel {
  public:
    RendererSettingsPanel();
    void OnImGuiRender() override;

  private:
    float m_FpsValues[50] = {};
    std::vector<float> m_FrameTimes{};
  };
}
