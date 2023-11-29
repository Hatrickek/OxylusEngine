#pragma once

#include <imgui.h>
#include <functional>
#include <optional>
#include <charconv>

#include "Core/Base.h"

#include "Utils/Log.h"

namespace Oxylus {
class RuntimeConsoleLogSink : public ExternalSink {
public:
  explicit RuntimeConsoleLogSink(void* user_data)
    : ExternalSink(user_data) {}

  void log(int64_t ns,
           fmtlog::LogLevel level,
           fmt::string_view location,
           size_t base_pos,
           fmt::string_view thread_name,
           fmt::string_view msg,
           size_t body_pos,
           size_t log_file_pos) override;
};

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

  bool render_menu_bar = false;
  bool set_focus_to_keyboard_always = false;
  const char* panel_name = "RuntimeConsole";
  bool visible = true;
  std::string id = {};

  RuntimeConsole();
  ~RuntimeConsole() = default;

  void register_command(const std::string& command, const std::string& on_succes_log, const std::function<void()>& action);
  void register_command(const std::string& command, const std::string& on_succes_log, int32_t* value);
  void register_command(const std::string& command, const std::string& on_succes_log, std::string* value);
  void register_command(const std::string& command, const std::string& on_succes_log, bool* value);

  void add_log(const char* fmt, fmtlog::LogLevel level);
  void clear_log();

  void on_imgui_render(ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoCollapse);

private:
  struct ConsoleText {
    std::string text = {};
    fmtlog::LogLevel level = {};

    void render() const;
  };

  struct ConsoleCommand {
    int32_t* int_value = nullptr;
    std::string* str_value = nullptr;
    bool* bool_value = nullptr;
    std::function<void()> action = nullptr;
    std::string on_succes_log = {};
  };

  // Sink
  Ref<RuntimeConsoleLogSink> runtime_console_log_sink;

  // Commands
  std::unordered_map<std::string, ConsoleCommand> m_command_map;
  void process_command(const std::string& command);

  void help_command();

  ParsedCommandValue parse_value(const std::string& command);
  std::string parse_command(const std::string& command);

  // Input field
  const int32_t MAX_TEXT_BUFFER_SIZE = 32;
  int32_t m_history_position = 0;
  std::vector<ConsoleText> m_text_buffer = {};
  std::vector<char*> m_input_log = {};
  bool m_request_scroll_to_bottom = true;
  int input_text_callback(ImGuiInputTextCallbackData* data);
};
}
