#pragma once

#include <imgui.h>
#include <functional>
#include <optional>
#include <charconv>

#include "Utils/Log.h"

namespace Oxylus {
class RuntimeConsole {
public:
  bool RenderMenuBar = false;
  bool SetFocusToKeyboardAlways = false;
  const char* PanelName = "RuntimeConsole";
  bool Visible = true;

  RuntimeConsole();
  ~RuntimeConsole() = default;

  // Call the `action` when the "command" is called.
  void RegisterCommand(const std::string& command,
                       const std::string& onSuccesLog,
                       const std::function<void()>& action);

  /// Change the `value` with the `command` \n
  /// example: "command 100" -> "command" is now 100
  void RegisterCommand(const std::string& command, const std::string& onSuccesLog, int32_t* value);

  /// Change the `value` with the `command` \n
  /// example: "command text" -> "command" is now text
  void RegisterCommand(const std::string& command, const std::string& onSuccesLog, std::string* value);

  /// Change the `value` with the `command` \n
  /// example: "command 1" -> "command" is now true
  void RegisterCommand(const std::string& command, const std::string& onSuccesLog, bool* value);

  void AddLog(const char* fmt, spdlog::level::level_enum level);
  void ClearLog();

  void OnImGuiRender(ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoCollapse);

private:
  struct ConsoleText {
    std::string Text = {};
    spdlog::level::level_enum Level = {};

    void Render() const;
  };

  struct ConsoleCommand {
    int32_t* IntValue = nullptr;
    std::string* StrValue = nullptr;
    bool* BoolValue = nullptr;
    std::function<void()> Action = nullptr;
    std::string OnSuccesLog = {};
  };

  // Commands
  std::unordered_map<std::string, ConsoleCommand> m_CommandMap;
  void ProcessCommand(const std::string& command);

  void HelpCommand();

  struct ParsedCommandValue {
    std::string StrValue = {};

    ParsedCommandValue(std::string str) : StrValue(std::move(str)) { }

    template <typename T= int32_t>
    std::optional<T> As() const {
      T value = {};
      if (std::from_chars(StrValue.c_str(), StrValue.data() + StrValue.size(), value).ec == std::errc{})
        return (T)value;
      return {};
    }
  };

  ParsedCommandValue ParseValue(const std::string& command);
  std::string ParseCommand(const std::string& command);

  // Input field
  const int32_t MAX_TEXT_BUFFER_SIZE = 32;
  int32_t m_HistoryPosition = 0;
  std::vector<ConsoleText> m_TextBuffer = {};
  std::vector<char*> m_InputLog = {};
  bool m_RequestScrollToBottom = true;
  int InputTextCallback(ImGuiInputTextCallbackData* data);
};
}
