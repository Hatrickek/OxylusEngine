#include "RuntimeConsole.h"

#include <icons/IconsMaterialDesignIcons.h>

#include "ImGuiLayer.h"

#include "Core/App.h"

#include "Utils/CVars.h"
#include "Utils/StringUtils.h"

namespace ox {
static ImVec4 get_color(const loguru::Verbosity verb) {
  switch (verb) {
    case loguru::Verbosity_INFO   : return {0, 1, 0, 1};
    case loguru::Verbosity_WARNING: return {0.9f, 0.6f, 0.2f, 1};
    case loguru::Verbosity_ERROR  : return {1, 0, 0, 1};
    default                       : return {1, 1, 1, 1};
  }
}

static const char8_t* get_level_icon(const loguru::Verbosity level) {
  switch (level) {
    case loguru::Verbosity_INFO   : return ICON_MDI_INFORMATION;
    case loguru::Verbosity_WARNING: return ICON_MDI_ALERT;
    case loguru::Verbosity_ERROR  : return ICON_MDI_CLOSE_OCTAGON;
    default                       :;
  }

  return u8"Unknown name";
}

RuntimeConsole::RuntimeConsole() {
  Log::add_callback("runtime_console", [](void* user_data, const loguru::Message& message) {
    const auto console = reinterpret_cast<RuntimeConsole*>(user_data);
    console->add_log(message.message, message.verbosity);
  }, this, loguru::Verbosity_INFO);

  // Default commands
  register_command("quit", "", [] { App::get()->close(); });
  register_command("clear", "", [this] { clear_log(); });
  register_command("help", "", [this] { help_command(); });

  request_scroll_to_bottom = true;
}

RuntimeConsole::~RuntimeConsole() {
  Log::remove_callback("runtime_console");
}

void RuntimeConsole::register_command(const std::string& command, const std::string& on_succes_log, const std::function<void()>& action) {
  command_map.emplace(command, ConsoleCommand{nullptr, nullptr, nullptr, action, on_succes_log});
}

void RuntimeConsole::register_command(const std::string& command, const std::string& on_succes_log, int32_t* value) {
  command_map.emplace(command, ConsoleCommand{value, nullptr, nullptr, nullptr, on_succes_log});
}

void RuntimeConsole::register_command(const std::string& command, const std::string& on_succes_log, std::string* value) {
  command_map.emplace(command, ConsoleCommand{nullptr, value, nullptr, nullptr, on_succes_log});
}

void RuntimeConsole::register_command(const std::string& command, const std::string& on_succes_log, bool* value) {
  command_map.emplace(command, ConsoleCommand{nullptr, nullptr, value, nullptr, on_succes_log});
}

void RuntimeConsole::add_log(const char* fmt, loguru::Verbosity verb) {
  if ((uint32_t)text_buffer.size() >= MAX_TEXT_BUFFER_SIZE)
    text_buffer.erase(text_buffer.begin());
  text_buffer.emplace_back(fmt, verb);
  request_scroll_to_bottom = true;
}

void RuntimeConsole::clear_log() { text_buffer.clear(); }

void RuntimeConsole::on_imgui_render() {
  if (ImGui::IsKeyPressed(ImGuiKey_GraveAccent, false)) {
    visible = !visible;
  }
  if (visible) {
    constexpr auto animation_duration = 0.5f;
    constexpr auto animation_speed = 3.0f;

    animation_counter += (float)App::get_timestep().get_seconds() * animation_speed;
    animation_counter = std::clamp(animation_counter, 0.0f, animation_duration);

    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->WorkPos, ImGuiCond_Always);
    ImVec2 size = {ImGui::GetMainViewport()->WorkSize.x, ImGui::GetMainViewport()->WorkSize.y * animation_counter};
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);

    constexpr ImGuiWindowFlags windowFlags =
      ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoCollapse;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.000f, 0.000f, 0.000f, 1.000f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.000f, 0.000f, 0.000f, 0.784f));
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0.100f, 0.100f, 0.100f, 1.000f));

    id = fmt::format(" {} {}\t\t###", StringUtils::from_char8_t(ICON_MDI_CONSOLE), panel_name);
    if (ImGui::Begin(id.c_str(), &visible, windowFlags)) {
      if (ImGui::BeginMenuBar()) {
        if (ImGui::MenuItem(StringUtils::from_char8_t(ICON_MDI_TRASH_CAN))) {
          clear_log();
        }
        if (ImGui::MenuItem(StringUtils::from_char8_t(ICON_MDI_INFORMATION), nullptr, text_filter == loguru::Verbosity_INFO)) {
          text_filter = text_filter == loguru::Verbosity_INFO ? loguru::Verbosity_OFF : loguru::Verbosity_INFO;
        }
        if (ImGui::MenuItem(StringUtils::from_char8_t(ICON_MDI_ALERT), nullptr, text_filter == loguru::Verbosity_WARNING)) {
          text_filter = text_filter == loguru::Verbosity_WARNING ? loguru::Verbosity_OFF : loguru::Verbosity_WARNING;
        }
        if (ImGui::MenuItem(StringUtils::from_char8_t(ICON_MDI_CLOSE_OCTAGON), nullptr, text_filter == loguru::Verbosity_ERROR)) {
          text_filter = text_filter == loguru::Verbosity_ERROR ? loguru::Verbosity_OFF : loguru::Verbosity_ERROR;
        }

        ImGui::EndMenuBar();
      }

      ImGui::Separator();

      constexpr ImGuiTableFlags table_flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_ContextMenuInBody | ImGuiTableFlags_ScrollY;

      float width = 0;
      if (ImGui::BeginChild("TextTable", ImVec2(0, -35))) {
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {1, 1});
        if (ImGui::BeginTable("ScrollRegionTable", 1, table_flags)) {
          width = ImGui::GetWindowSize().x;
          ImGui::PushFont(ImGuiLayer::bold_font);
          for (uint32_t i = 0; i < (uint32_t)text_buffer.size(); i++) {
            if (text_filter != loguru::Verbosity_OFF && text_filter != text_buffer[i].verbosity)
              continue;
            render_console_text(text_buffer[i].text, text_buffer[i].verbosity);
          }

          ImGui::PopFont();
          if (request_scroll_to_bottom) {
            ImGui::SetScrollY(ImGui::GetScrollMaxY() * 10);
            request_scroll_to_bottom = false;
          }
          ImGui::EndTable();
        }
        ImGui::PopStyleVar();
      }
      ImGui::EndChild();

      ImGui::Separator();
      ImGui::PushItemWidth(width - 10);
      constexpr ImGuiInputTextFlags input_flags =
        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_EscapeClearsAll;
      static char s_input_buf[256];
      ImGui::PushFont(ImGuiLayer::bold_font);

      auto callback = [](ImGuiInputTextCallbackData* data) {
        const auto panel = (RuntimeConsole*)data->UserData;
        return panel->input_text_callback(data);
      };

      ImGui::SetKeyboardFocusHere();
      if (ImGui::InputText("##", s_input_buf, std::size(s_input_buf), input_flags, callback, this)) {
        process_command(s_input_buf);
        input_log.emplace_back(s_input_buf);
        memset(s_input_buf, 0, sizeof s_input_buf);
      }

      ImGui::PopFont();
      ImGui::PopItemWidth();
    }
    ImGui::End();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
  } else {
    animation_counter = 0.0f;
  }
}

void RuntimeConsole::render_console_text(const std::string& text, loguru::Verbosity verb) {
  ImGui::TableNextRow();
  ImGui::TableNextColumn();

  ImGuiTreeNodeFlags flags = 0;
  flags |= ImGuiTreeNodeFlags_OpenOnArrow;
  flags |= ImGuiTreeNodeFlags_SpanFullWidth;
  flags |= ImGuiTreeNodeFlags_FramePadding;
  flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

  ImGui::PushID(text.c_str());
  ImGui::PushStyleColor(ImGuiCol_Text, get_color(verb));
  const auto level_icon = get_level_icon(verb);
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
void log_cvar_change(RuntimeConsole* console, const char* cvar_name, T current_value, bool changed) {
  const std::string log_text = changed ? fmt::format("Changed {} to {}", cvar_name, current_value) : fmt::format("{} {}", cvar_name, current_value);
  console->add_log(log_text.c_str(), loguru::Verbosity_INFO);
}

void RuntimeConsole::process_command(const std::string& command) {
  const auto parsed_command = parse_command(command);
  const auto value = parse_value(command);

  bool is_cvar_variable = false;

  auto* cvar_system = CVarSystem::get();
  const auto cvar = cvar_system->get_cvar(entt::hashed_string(parsed_command.c_str()));
  if (cvar) {
    is_cvar_variable = true;
    switch (cvar->type) {
      case CVarType::INT: {
        auto current_value = cvar_system->int_cvars.at(cvar->array_index).current;
        bool changed = false;
        if (!value.str_value.empty()) {
          const auto parsed = value.as<int32_t>();
          if (parsed.has_value()) {
            cvar_system->set_int_cvar(entt::hashed_string(cvar->name.c_str()), *parsed);
            current_value = *parsed;
            changed = true;
          }
        }
        log_cvar_change<int32_t>(this, cvar->name.c_str(), current_value, changed);
        break;
      }
      case CVarType::FLOAT: {
        auto current_value = cvar_system->float_cvars.at(cvar->array_index).current;
        bool changed = false;
        if (!value.str_value.empty()) {
          const auto parsed = value.as<float>();
          if (parsed.has_value()) {
            cvar_system->set_float_cvar(entt::hashed_string(cvar->name.c_str()), *parsed);
            current_value = *parsed;
            changed = true;
          }
        }
        log_cvar_change<float>(this, cvar->name.c_str(), current_value, changed);
        break;
      }
      case CVarType::STRING: {
        auto current_value = cvar_system->string_cvars.at(cvar->array_index).current;
        bool changed = false;
        if (!value.str_value.empty()) {
          cvar_system->set_string_cvar(entt::hashed_string(cvar->name.c_str()), value.str_value.c_str());
          current_value = value.str_value;
          changed = true;
        }
        log_cvar_change<std::string>(this, cvar->name.c_str(), current_value, changed);
        break;
      }
    }
  }

  // commands registered with register_command()
  if (command_map.contains(parsed_command)) {
    const auto& c = command_map[parsed_command];
    if (c.action != nullptr) {
      c.action();
    }
    if (!value.str_value.empty()) {
      if (c.str_value != nullptr) {
        *c.str_value = value.str_value;
      } else if (c.int_value != nullptr) {
        const auto v = value.as<int32_t>();
        if (v.has_value()) {
          *c.int_value = v.value();
        }
      } else if (c.bool_value != nullptr) {
        const auto v = value.as<int32_t>();
        if (v.has_value()) {
          *c.bool_value = v.value();
        }
      }
    }
    if (!c.on_succes_log.empty())
      add_log(c.on_succes_log.c_str(), loguru::Verbosity_INFO);
  } else {
    if (!is_cvar_variable)
      add_log("Non existent command.", loguru::Verbosity_ERROR);
  }
}

RuntimeConsole::ParsedCommandValue RuntimeConsole::parse_value(const std::string& command) {
  if (command.find(' ') == std::string::npos)
    return {""};
  const auto offset = command.find(' ');
  const auto value = command.substr(offset + 1, command.size() - offset);
  return {value};
}

std::string RuntimeConsole::parse_command(const std::string& command) { return command.substr(0, command.find(' ')); }

int RuntimeConsole::input_text_callback(ImGuiInputTextCallbackData* data) {
  if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
    const int prev_history_pos = history_position;
    if (data->EventKey == ImGuiKey_UpArrow) {
      if (history_position == -1)
        history_position = (int32_t)input_log.size() - 1;
      else if (history_position > 0)
        history_position--;
    } else if (data->EventKey == ImGuiKey_DownArrow) {
      if (history_position != -1)
        if (++history_position >= (int32_t)input_log.size())
          history_position = -1;
    }

    if (prev_history_pos != history_position) {
      const char* history_str = history_position >= 0 ? input_log[history_position] : "";
      data->DeleteChars(0, data->BufTextLen);
      data->InsertChars(0, history_str);
    }
  }

  return 0;
}

void RuntimeConsole::help_command() {
  std::string available_commands = "Available commands: \n";
  for (auto& [commandStr, command] : command_map) {
    available_commands.append(fmt::format("\t{0} \n", commandStr));
  }

  const auto system = CVarSystem::get();
  for (const auto& var : system->int_cvars) {
    const auto c = fmt::format("\t{0} \n", var.parameter->name);
    available_commands.append(c);
  }

  for (const auto& var : system->float_cvars) {
    const auto c = fmt::format("\t{0} \n", var.parameter->name);
    available_commands.append(c);
  }

  add_log(available_commands.c_str(), loguru::Verbosity_INFO);
}
} // namespace ox
