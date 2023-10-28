#pragma once

#include <imgui.h>
#include <functional>
#include <optional>
#include <charconv>

#include "Utils/Log.h"

namespace Oxylus {
using LevelEnum = spdlog::level::level_enum;

class RuntimeConsole {
public:
  struct ParsedCommandValue {
    std::string str_value = {};

    ParsedCommandValue(std::string str) : str_value(std::move(str)) { }

    template <typename T = int32_t>
    std::optional<T> as() const {
      T value = {};
      if (std::from_chars(str_value.c_str(), str_value.data() + str_value.size(), value).ec == std::errc{})
        return (T)value;
      return {};
    }
  };

  bool RenderMenuBar = false;
  bool SetFocusToKeyboardAlways = false;
  const char* PanelName = "RuntimeConsole";
  bool Visible = true;

  RuntimeConsole();
  ~RuntimeConsole() = default;

  void RegisterCommand(const std::string& command, const std::string& onSuccesLog, const std::function<void()>& action);
  void RegisterCommand(const std::string& command, const std::string& onSuccesLog, int32_t* value);
  void RegisterCommand(const std::string& command, const std::string& onSuccesLog, std::string* value);
  void RegisterCommand(const std::string& command, const std::string& onSuccesLog, bool* value);

  void add_log(const char* fmt, spdlog::level::level_enum level);
  void ClearLog();

  void OnImGuiRender(ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoCollapse);

private:
  struct ConsoleText {
    std::string Text = {};
    spdlog::level::level_enum Level = {};

    void Render() const;
  };

  struct ConsoleCommand {
    int32_t* int_value = nullptr;
    std::string* str_value = nullptr;
    bool* bool_value = nullptr;
    std::function<void()> action = nullptr;
    std::string on_succes_log = {};
  };

  // Commands
  std::unordered_map<std::string, ConsoleCommand> m_CommandMap;
  void process_command(const std::string& command);

  void HelpCommand();

  ParsedCommandValue parse_value(const std::string& command);
  std::string parse_command(const std::string& command);

  // Input field
  const int32_t MAX_TEXT_BUFFER_SIZE = 32;
  int32_t m_HistoryPosition = 0;
  std::vector<ConsoleText> m_TextBuffer = {};
  std::vector<char*> m_InputLog = {};
  bool m_RequestScrollToBottom = true;
  int InputTextCallback(ImGuiInputTextCallbackData* data);
};
}
