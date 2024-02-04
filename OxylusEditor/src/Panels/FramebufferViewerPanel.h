#pragma once

#include "EditorPanel.h"

namespace vk {
  class DescriptorSet;
}

namespace Ox {
  class FramebufferViewerPanel : public EditorPanel {
  public:
    FramebufferViewerPanel();

    void on_imgui_render() override;

  private:
    void DrawViewer(void* imageDescriptorSet) const;
  };
}
