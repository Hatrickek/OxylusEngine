#include "ConsolePanel.h"

#include "imgui.h"
#include <vector>
#include <icons/IconsMaterialDesignIcons.h>

#include "Assets/AssetManager.h"
#include "spdlog/common.h"
#include "Core/Input.h"
#include "UI/ExternalConsoleSink.h"
#include "UI/IGUI.h"
#include "Utils/StringUtils.h"

namespace Oxylus {
  enum ConsoleCommands {
    COMMAND_CLEAR_CONSOLE,
    COMMAND_IMGUI_STYLE_EDITOR,
    COMMAND_IMGUI_DEMO_WINDOW,
    COMMAND_OPEN_PROJECT,
    COMMAND_SHOW_FPS,
    COMMAND_CLEAR_ASSETS,
  };

  static ImVec4 GetColor(const spdlog::level::level_enum level) {
    switch (level) {
      case SPDLOG_LEVEL_INFO: return {1, 1, 1, 1};
      case SPDLOG_LEVEL_WARN: return {0.2f, 0, 0, 1};
      case SPDLOG_LEVEL_ERROR: return {1, 0, 0, 1};
      default: return {1, 1, 1, 1};
    }
  }

  void ConsolePanel::AddLog(const char* fmt, spdlog::level::level_enum level) {
    ConsoleText text;
    text.Text = fmt;
    text.Level = level;
    m_Buffer.emplace_back(text);

    m_RequestScrollToBottom = true;
  }

  void ConsolePanel::ClearLog() {
    m_Buffer.clear();
  }

  static const char8_t* GetLevelIcon(spdlog::level::level_enum level) {
    switch (level) {
      case spdlog::level::trace: return ICON_MDI_MESSAGE_TEXT;
      case spdlog::level::debug: return ICON_MDI_BUG;
      case spdlog::level::info: return ICON_MDI_INFORMATION;
      case spdlog::level::warn: return ICON_MDI_ALERT;
      case spdlog::level::err: return ICON_MDI_CLOSE_OCTAGON;
      case spdlog::level::critical: return ICON_MDI_ALERT_OCTAGRAM;
      default:;
    }

    return u8"Unknown name";
  }

  void ConsolePanel::ConsoleText::Render() const {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    ImGuiTreeNodeFlags flags = 0;
    flags |= ImGuiTreeNodeFlags_OpenOnArrow;
    flags |= ImGuiTreeNodeFlags_SpanFullWidth;
    flags |= ImGuiTreeNodeFlags_FramePadding;
    flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    ImGui::PushID(Text.c_str());
    ImGui::PushStyleColor(ImGuiCol_Text, GetColor(Level));
    const auto levelIcon = GetLevelIcon(Level);
    ImGui::TreeNodeEx(Text.c_str(), flags, "%s  %s", StringUtils::FromChar8T(levelIcon), Text.c_str());
    ImGui::PopStyleColor();

    if (ImGui::BeginPopupContextItem("Popup")) {
      if (ImGui::MenuItem("Copy"))
        ImGui::SetClipboardText(Text.c_str());

      ImGui::EndPopup();
    }
    ImGui::PopID();
  }

  static void ProcessCommands(ConsolePanel& consolePanel, ConsoleCommands command) {
    switch (command) {
      case COMMAND_IMGUI_STYLE_EDITOR: {
        for (auto& event : consolePanel.Events.StyleEditorEvent)
          event();
        break;
      }
      case COMMAND_IMGUI_DEMO_WINDOW: {
        for (auto& event : consolePanel.Events.DemoWindowEvent)
          event();
        break;
      }
      case COMMAND_OPEN_PROJECT: break;
      case COMMAND_SHOW_FPS: break;
      case COMMAND_CLEAR_CONSOLE: {
        consolePanel.ClearLog();
        break;
      }
      case COMMAND_CLEAR_ASSETS: {
        AssetManager::FreeUnusedAssets();
        break;
      }
    }
  }

  static void ProcessInput(ConsolePanel& consolePanel, const std::string& input) {
    if (input == "imgui_show_style_editor") {
      ProcessCommands(consolePanel, COMMAND_IMGUI_STYLE_EDITOR);
    }
    else if (input == "imgui_show_demo_window") {
      ProcessCommands(consolePanel, COMMAND_IMGUI_DEMO_WINDOW);
    }
    else if (input == "open_project") {
      ProcessCommands(consolePanel, COMMAND_OPEN_PROJECT);
    }
    else if (input == "show_fps") {
      ProcessCommands(consolePanel, COMMAND_SHOW_FPS);
    }
    else if (input == "clear") {
      ProcessCommands(consolePanel, COMMAND_CLEAR_CONSOLE);
    }
    else if (input == "clear_assets") {
      ProcessCommands(consolePanel, COMMAND_CLEAR_ASSETS);
    }
    else {
      consolePanel.AddLog("Non existent command.", spdlog::level::level_enum::err);
    }
  }

  ConsolePanel::ConsolePanel() : EditorPanel("Console", ICON_MDI_CONSOLE, true) {
    ExternalConsoleSink::SetConsoleSink_HandleFlush([this](std::string_view message,
                                                           const char*,
                                                           const char*,
                                                           int32_t,
                                                           spdlog::level::level_enum level) {
      AddLog(message.data(), level);
    });
  }

  int ConsolePanel::InputTextCallback(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
      const int prevHistoryPos = m_HistoryPosition;
      if (data->EventKey == ImGuiKey_UpArrow) {
        if (m_HistoryPosition == -1)
          m_HistoryPosition = (int32_t)m_InputLog.size() - 1;
        else if (m_HistoryPosition > 0)
          m_HistoryPosition--;
      }
      else if (data->EventKey == ImGuiKey_DownArrow) {
        if (m_HistoryPosition != -1)
          if (++m_HistoryPosition >= (int32_t)m_InputLog.size())
            m_HistoryPosition = -1;
      }

      if (prevHistoryPos != m_HistoryPosition) {
        const char* historyStr = m_HistoryPosition >= 0 ? m_InputLog[m_HistoryPosition] : "";
        data->DeleteChars(0, data->BufTextLen);
        data->InsertChars(0, historyStr);
      }
    }

    return 0;
  }

  void ConsolePanel::OnImGuiRender() {
    if (!Visible)
      return;
    constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNavInputs |
                                             ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_AlwaysAutoResize |
                                             ImGuiWindowFlags_NoScrollbar;

    static char inputBuf[256];
    if (OnBegin(windowFlags)) {
      const float width = ImGui::GetWindowWidth();
      const float height = ImGui::GetWindowHeight();
      if (ImGui::BeginMenuBar()) {
        if (ImGui::MenuItem(StringUtils::FromChar8T(ICON_MDI_TRASH_CAN))) {
          ClearLog();
        }
        ImGui::EndMenuBar();
      }
      ImGui::Separator();

      constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_ContextMenuInBody |
                                             ImGuiTableFlags_ScrollY;

      ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {1, 1});
      if (ImGui::BeginTable("ScrollRegionTable", 1, tableFlags)) {
        for (auto& text : m_Buffer) {
          text.Render();
        }
        if (m_RequestScrollToBottom && ImGui::GetScrollMaxY() > 0) {
          ImGui::SetScrollY(ImGui::GetScrollMaxY());
          m_RequestScrollToBottom = false;
        }
        ImGui::EndTable();
      }
      ImGui::PopStyleVar();
      ImGui::Separator();
      ImGui::PushItemWidth(width - 10);
      constexpr ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue |
                                                 ImGuiInputTextFlags_CallbackHistory |
                                                 ImGuiInputTextFlags_EscapeClearsAll;
      if (ImGui::InputText("##",
                           inputBuf,
                           OX_ARRAYSIZE(inputBuf),
                           inputFlags,
                           [](ImGuiInputTextCallbackData* data) {
                             const auto panel = (ConsolePanel*)data->UserData;
                             return panel->InputTextCallback(data);
                           },
                           this)) {
        ProcessInput(*this, inputBuf);
        m_InputLog.emplace_back(inputBuf);
        memset(inputBuf, 0, sizeof inputBuf);
      }
      ImGui::PopItemWidth();

      OnEnd();
    }
  }
}
