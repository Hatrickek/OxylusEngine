#pragma once

#include "EditorPanel.h"

namespace Oxylus {
  class ProjectPanel : public EditorPanel {
  public:
    ProjectPanel();
    ~ProjectPanel() override = default;

    void on_update() override;
    void on_imgui_render() override;
  private:
    void LoadProjectForEditor(const std::string& filepath);
  };
}
