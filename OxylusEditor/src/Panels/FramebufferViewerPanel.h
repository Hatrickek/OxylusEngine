#pragma once
#include "EditorPanel.h"

namespace vk {
  class DescriptorSet;
}

namespace Oxylus {
  class FramebufferViewerPanel : public EditorPanel {
  public:
    FramebufferViewerPanel();

    void OnImGuiRender() override;

  private:
    void DrawViewer(vk::DescriptorSet imageDescriptorSet) const;
  };
}
