#pragma once

#include <imgui.h>
#include <functional>
#include "Utils/Log.h"

namespace Oxylus {
  class RuntimeConsole {
  public:

    bool RenderMenuBar = false;
    const char* PanelName = "RuntimeConsole";
    bool Visible = true;

    RuntimeConsole();
    ~RuntimeConsole() = default;

    void RegisterCommand(const std::string& command, const std::function<void()>& action);
    void AddLog(const char* fmt, spdlog::level::level_enum level);
    void ClearLog();

    void OnImGuiRender();
  private:
    struct ConsoleText {
      std::string Text = {};
      spdlog::level::level_enum Level = {};

      void Render() const;
    };

    // Commands
    std::unordered_map<std::string, std::function<void()>> m_CommandMap;
    void ProcessCommand(const std::string& command);

    // Input field
    int32_t m_HistoryPosition = 0;
    std::vector<ConsoleText> m_TextBuffer = {};
    std::vector<char*> m_InputLog = {};
    bool m_RequestScrollToBottom = false;
    int InputTextCallback(ImGuiInputTextCallbackData* data);
  };
}
