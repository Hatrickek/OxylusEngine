#include "RuntimeConsole.h"

#include <icons/IconsMaterialDesignIcons.h>

#include "Core/Application.h"

#include "Utils/CVars.h"
#include "Utils/StringUtils.h"

namespace Oxylus {
static ImVec4 get_color(const fmtlog::LogLevel level) {
  switch (level) {
    case fmtlog::LogLevel::INF: return {0, 1, 0, 1};
    case fmtlog::LogLevel::WRN: return {0.9f, 0.6f, 0.2f, 1};
    case fmtlog::LogLevel::ERR: return {1, 0, 0, 1};
    case fmtlog::LogLevel::DBG: return {1, 1, 1, 1};
    default: return {1, 1, 1, 1};
  }
}

static const char8_t* get_level_icon(fmtlog::LogLevel level) {
  switch (level) {
    case fmtlog::LogLevel::DBG: return ICON_MDI_MESSAGE_TEXT;
    case fmtlog::LogLevel::INF: return ICON_MDI_INFORMATION;
    case fmtlog::LogLevel::WRN: return ICON_MDI_ALERT;
    case fmtlog::LogLevel::ERR: return ICON_MDI_CLOSE_OCTAGON;
    default: ;
  }

  return u8"Unknown name";
}

void RuntimeConsoleLogSink::log(int64_t ns,
                                const fmtlog::LogLevel level,
                                fmt::string_view location,
                                size_t base_pos,
                                fmt::string_view thread_name,
                                const fmt::string_view msg,
                                size_t body_pos,
                                size_t log_file_pos) {
  const auto console = reinterpret_cast<RuntimeConsole*>(user_data);
  console->add_log(msg.data(), level);
}

RuntimeConsole::RuntimeConsole() {
  Log::register_sink<RuntimeConsoleLogSink>(this);

  // Default commands
  register_command("quit", "", [] { Application::get()->close(); });
  register_command("clear", "", [this] { clear_log(); });
  register_command("help", "", [this] { help_command(); });

  m_request_scroll_to_bottom = true;
}

void RuntimeConsole::register_command(const std::string& command,
                                      const std::string& on_succes_log,
                                      const std::function<void()>& action) {
  m_command_map.emplace(command, ConsoleCommand{nullptr, nullptr, nullptr, action, on_succes_log});
}

void RuntimeConsole::register_command(const std::string& command, const std::string& on_succes_log, int32_t* value) {
  m_command_map.emplace(command, ConsoleCommand{value, nullptr, nullptr, nullptr, on_succes_log});
}

void RuntimeConsole::register_command(const std::string& command, const std::string& on_succes_log, std::string* value) {
  m_command_map.emplace(command, ConsoleCommand{nullptr, value, nullptr, nullptr, on_succes_log});
}

void RuntimeConsole::register_command(const std::string& command, const std::string& on_succes_log, bool* value) {
  m_command_map.emplace(command, ConsoleCommand{nullptr, nullptr, value, nullptr, on_succes_log});
}

void RuntimeConsole::add_log(const char* fmt, fmtlog::LogLevel level) {
  if ((int32_t)m_text_buffer.size() >= MAX_TEXT_BUFFER_SIZE)
    m_text_buffer.erase(m_text_buffer.begin());
  m_text_buffer.emplace_back(ConsoleText{fmt, level});
  m_request_scroll_to_bottom = true;
}

void RuntimeConsole::clear_log() {
  m_text_buffer.clear();
}

void RuntimeConsole::on_imgui_render(ImGuiWindowFlags window_flags) {
  if (!visible)
    return;
  id = fmt::format(" {} {}\t\t###", StringUtils::from_char8_t(ICON_MDI_CONSOLE), panel_name);
  if (ImGui::Begin(id.c_str(), &visible, window_flags)) {
    if (render_menu_bar) {
      if (ImGui::BeginMenuBar()) {
        if (ImGui::MenuItem(StringUtils::from_char8_t(ICON_MDI_TRASH_CAN))) {
          clear_log();
        }
        ImGui::EndMenuBar();
      }
      ImGui::Separator();
    }

    constexpr ImGuiTableFlags table_flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_ContextMenuInBody |
                                            ImGuiTableFlags_ScrollY;

    float width = 0;
    if (ImGui::BeginChild("TextTable", ImVec2(0, -35))) {
      ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {1, 1});
      if (ImGui::BeginTable("ScrollRegionTable", 1, table_flags)) {
        width = ImGui::GetWindowSize().x;
        ImGui::PushFont(ImGuiLayer::BoldFont);
        for (auto& text : m_text_buffer) {
          text.render();
        }
        ImGui::PopFont();
        if (m_request_scroll_to_bottom) {
          ImGui::SetScrollY(ImGui::GetScrollMaxY() * 10);
          m_request_scroll_to_bottom = false;
        }
        ImGui::EndTable();
      }
      ImGui::PopStyleVar();
    }
    ImGui::EndChild();

    ImGui::Separator();
    ImGui::PushItemWidth(width - 10);
    constexpr ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_EnterReturnsTrue |
                                                ImGuiInputTextFlags_CallbackHistory |
                                                ImGuiInputTextFlags_EscapeClearsAll;
    static char s_input_buf[256];
    ImGui::PushFont(ImGuiLayer::BoldFont);
    if (set_focus_to_keyboard_always)
      ImGui::SetKeyboardFocusHere();

    auto callback = [](ImGuiInputTextCallbackData* data) {
      const auto panel = (RuntimeConsole*)data->UserData;
      return panel->input_text_callback(data);
    };

    if (ImGui::InputText("##", s_input_buf,OX_ARRAYSIZE(s_input_buf), input_flags, callback, this)) {
      process_command(s_input_buf);
      m_input_log.emplace_back(s_input_buf);
      memset(s_input_buf, 0, sizeof s_input_buf);
    }

    ImGui::PopFont();
    ImGui::PopItemWidth();
  }
  ImGui::End();
}

void RuntimeConsole::ConsoleText::render() const {
  ImGui::TableNextRow();
  ImGui::TableNextColumn();

  ImGuiTreeNodeFlags flags = 0;
  flags |= ImGuiTreeNodeFlags_OpenOnArrow;
  flags |= ImGuiTreeNodeFlags_SpanFullWidth;
  flags |= ImGuiTreeNodeFlags_FramePadding;
  flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

  if (text.empty())
    return;

  ImGui::PushID(text.c_str());
  ImGui::PushStyleColor(ImGuiCol_Text, get_color(level));
  const auto level_icon = get_level_icon(level);
  ImGui::TreeNodeEx(text.c_str(), flags, "%s  %s", StringUtils::from_char8_t(level_icon), text.c_str());
  ImGui::PopStyleColor();

  if (ImGui::BeginPopupContextItem("Popup")) {
    if (ImGui::MenuItem("Copy"))
      ImGui::SetClipboardText(text.c_str());

    ImGui::EndPopup();
  }
  ImGui::PopID();
}

template <typename T>
T get_current_cvar_value(uint32_t cvar_array_index) {
  auto current_value = CVarSystem::get()->get_cvar_array<T>()->cvars[cvar_array_index].current;

  return current_value;
}

template <typename T>
void log_cvar_change(RuntimeConsole* console, const char* cvar_name, T current_value, bool changed) {
  const std::string log_text = changed
                                 ? fmt::format("Changed {} to {}", cvar_name, current_value)
                                 : fmt::format("{} {}", cvar_name, current_value);
  const fmtlog::LogLevel log_level = changed ? fmtlog::LogLevel::INF : fmtlog::LogLevel::DBG;
  console->add_log(log_text.c_str(), log_level);
}

void RuntimeConsole::process_command(const std::string& command) {
  const auto parsed_command = parse_command(command);
  const auto value = parse_value(command);

  bool is_cvar_variable = false;

  const auto cvar = CVarSystem::get()->get_cvar(parsed_command.c_str());
  if (cvar) {
    is_cvar_variable = true;
    switch (cvar->type) {
      case CVarType::INT: {
        auto current_value = get_current_cvar_value<int32_t>(cvar->arrayIndex);
        bool changed = false;
        if (!value.str_value.empty()) {
          const auto parsed = value.as<int32_t>();
          if (parsed.has_value()) {
            CVarSystem::get()->set_int_cvar(cvar->name.c_str(), *parsed);
            current_value = *parsed;
            changed = true;
          }
        }
        log_cvar_change<int32_t>(this, cvar->name.c_str(), current_value, changed);
        break;
      }
      case CVarType::FLOAT: {
        auto current_value = get_current_cvar_value<float>(cvar->arrayIndex);
        bool changed = false;
        if (!value.str_value.empty()) {
          const auto parsed = value.as<float>();
          if (parsed.has_value()) {
            CVarSystem::get()->set_float_cvar(cvar->name.c_str(), *parsed);
            current_value = *parsed;
            changed = true;
          }
        }
        log_cvar_change<float>(this, cvar->name.c_str(), current_value, changed);
        break;
      }
      case CVarType::STRING: {
        auto current_value = get_current_cvar_value<std::string>(cvar->arrayIndex);
        bool changed = false;
        if (!value.str_value.empty()) {
          CVarSystem::get()->set_string_cvar(cvar->name.c_str(), value.str_value.c_str());
          current_value = value.str_value;
          changed = true;
        }
        log_cvar_change<std::string>(this, cvar->name.c_str(), current_value, changed);
        break;
      }
    }
  }

  // commands registered with register_command()
  if (m_command_map.contains(parsed_command)) {
    const auto& c = m_command_map[parsed_command];
    if (c.action != nullptr) {
      c.action();
    }
    if (!value.str_value.empty()) {
      if (c.str_value != nullptr) {
        *c.str_value = value.str_value;
      }
      else if (c.int_value != nullptr) {
        const auto v = value.as<int32_t>();
        if (v.has_value()) {
          *c.int_value = v.value();
        }
      }
      else if (c.bool_value != nullptr) {
        const auto v = value.as<int32_t>();
        if (v.has_value()) {
          *c.bool_value = v.value();
        }
      }
    }
    if (!c.on_succes_log.empty())
      add_log(c.on_succes_log.c_str(), fmtlog::LogLevel::INF);
  }
  else {
    if (!is_cvar_variable)
      add_log("Non existent command.", fmtlog::LogLevel::ERR);
  }
}

RuntimeConsole::ParsedCommandValue RuntimeConsole::parse_value(const std::string& command) {
  if (command.find(' ') == std::string::npos)
    return {""};
  const auto offset = command.find(' ');
  const auto value = command.substr(offset + 1, command.size() - offset);
  return {value};
}

std::string RuntimeConsole::parse_command(const std::string& command) {
  return command.substr(0, command.find(' '));
}

int RuntimeConsole::input_text_callback(ImGuiInputTextCallbackData* data) {
  if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
    const int prev_history_pos = m_history_position;
    if (data->EventKey == ImGuiKey_UpArrow) {
      if (m_history_position == -1)
        m_history_position = (int32_t)m_input_log.size() - 1;
      else if (m_history_position > 0)
        m_history_position--;
    }
    else if (data->EventKey == ImGuiKey_DownArrow) {
      if (m_history_position != -1)
        if (++m_history_position >= (int32_t)m_input_log.size())
          m_history_position = -1;
    }

    if (prev_history_pos != m_history_position) {
      const char* history_str = m_history_position >= 0 ? m_input_log[m_history_position] : "";
      data->DeleteChars(0, data->BufTextLen);
      data->InsertChars(0, history_str);
    }
  }

  return 0;
}

void RuntimeConsole::help_command() {
  std::string available_commands = "Available commands: \n";
  for (auto& [commandStr, command] : m_command_map) {
    available_commands.append(fmt::format("\t{0} \n", commandStr));
  }

  for (int i = 0; i < CVarSystem::get()->get_cvar_array<int32_t>()->lastCVar; i++) {
    const auto p = CVarSystem::get()->get_cvar_array<int32_t>()->cvars[i].parameter;
    available_commands.append(fmt::format("\t{0} \n", p->name.c_str()));
  }

  for (int i = 0; i < CVarSystem::get()->get_cvar_array<float>()->lastCVar; i++) {
    const auto p = CVarSystem::get()->get_cvar_array<int32_t>()->cvars[i].parameter;
    available_commands.append(fmt::format("\t{0} \n", p->name.c_str()));
  }

  add_log(available_commands.c_str(), fmtlog::LogLevel::DBG);
}
}
