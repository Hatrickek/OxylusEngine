#pragma once

#include "EditorPanel.h"
#include "imgui.h"
#include "Utils/Log.h"

namespace Oxylus {
  class ConsolePanel : public EditorPanel {
  public:
    struct Events {
      std::vector<std::function<void()>> StyleEditorEvent;
      std::vector<std::function<void()>> DemoWindowEvent;
    } Events;

    ConsolePanel();
    ~ConsolePanel() override = default;

    void OnImGuiRender() override;
    void AddLog(const char* fmt, spdlog::level::level_enum level);
    void ClearLog();

  private:
    struct ConsoleText {
      std::string Text = {};
      spdlog::level::level_enum Level = {};

      void Render() const;
    };

    int InputTextCallback(ImGuiInputTextCallbackData* data);
    int32_t m_HistoryPosition = 0;

    std::vector<ConsoleText> m_Buffer;
    std::vector<char*> m_InputLog;
    bool m_RequestScrollToBottom = false;
  };
}
