#include "EditorPanel.h"

#include "imgui.h"
#include "Utils/StringUtils.h"
#include <fmt/format.h>

namespace Oxylus {
  uint32_t EditorPanel::s_Count = 0;

  EditorPanel::EditorPanel(const char* name, const char8_t* icon, bool defaultShow) : Visible(defaultShow),
                                                                                      m_Name(name), m_Icon(icon) {
    m_ID = fmt::format(" {} {}\t\t###{}{}", StringUtils::FromChar8T(icon), name, s_Count, name);
    s_Count++;
  }

  bool EditorPanel::OnBegin(int32_t windowFlags) {
    if (!Visible)
      return false;

    ImGui::SetNextWindowSize(ImVec2(480, 640), ImGuiCond_FirstUseEver);

    ImGui::Begin(m_ID.c_str(), &Visible, windowFlags | ImGuiWindowFlags_NoCollapse);
    //ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 2.0f));
    return true;
  }

  void EditorPanel::OnEnd() const {
    //ImGui::PopStyleVar();
    ImGui::End();
  }
}
