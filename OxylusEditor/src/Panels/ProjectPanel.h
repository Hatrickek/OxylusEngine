#pragma once

#include "EditorPanel.h"

namespace Oxylus {
  class ProjectPanel : public EditorPanel {
  public:
    ProjectPanel();
    ~ProjectPanel() override = default;

    void OnUpdate() override;
    void OnImGuiRender() override;
  private:
    void LoadProjectForEditor(const std::string& filepath);
  };
}
