#include "EditorPanel.h"

#include "imgui.h"
#include "Utils/StringUtils.h"
#include <fmt/format.h>

namespace Oxylus {
  uint32_t EditorPanel::s_Count = 0;

  EditorPanel::EditorPanel(const char* name, const char8_t* icon, bool defaultShow)
    : Visible(defaultShow), m_Name(name), m_Icon(icon) {
    m_ID = fmt::format(" {} {}\t\t###{}{}", StringUtils::from_char8_t(icon), name, s_Count, name);
    s_Count++;
  }

  bool EditorPanel::on_begin(int32_t window_flags) {
    if (!Visible)
      return false;

    ImGui::SetNextWindowSize(ImVec2(480, 640), ImGuiCond_FirstUseEver);

    ImGui::Begin(m_ID.c_str(), &Visible, window_flags | ImGuiWindowFlags_NoCollapse);

    return true;
  }

  void EditorPanel::on_end() const {
    ImGui::End();
  }
}
