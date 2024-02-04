#pragma once

#include "UI/RuntimeConsole.h"

namespace Ox {
  class ConsolePanel {
  public:
    RuntimeConsole runtime_console = {};

    ConsolePanel();
    ~ConsolePanel() = default;

    void on_imgui_render();
  };
}
