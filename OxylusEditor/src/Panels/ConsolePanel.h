#pragma once

#include "UI/RuntimeConsole.h"

namespace Oxylus {
  class ConsolePanel {
  public:
    RuntimeConsole m_RuntimeConsole = {};

    ConsolePanel();
    ~ConsolePanel() = default;

    void OnImGuiRender();
  };
}
