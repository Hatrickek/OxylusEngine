#include "ProjectPanel.h"

#include <icons/IconsMaterialDesignIcons.h>
#include <imgui.h>

#include "EditorLayer.h"
#include "Assets/AssetManager.h"
#include "Core/Project.h"
#include "UI/IGUI.h"
#include "Utils/EditorConfig.h"
#include "Utils/UIUtils.h"

namespace Oxylus {
  ProjectPanel::ProjectPanel() : EditorPanel("Projects", ICON_MDI_ACCOUNT_BADGE, true) { }

  void ProjectPanel::OnUpdate() { }

  void ProjectPanel::LoadProjectForEditor(const std::string& filepath) {
    if (Project::Load(filepath)) {
      const auto projectDir = Project::GetProjectDirectory();
      const auto startScene = AssetManager::GetAssetFileSystemPath(Project::GetActive()->GetConfig().StartScene);
      EditorLayer::Get()->OpenScene(startScene);
      EditorConfig::Get()->AddRecentProject(filepath);
      Visible = false;
    }
  }

  void ProjectPanel::OnImGuiRender() {
    if (Visible)
      ImGui::OpenPopup("ProjectSelector");

    ImGui::SetNextWindowSize(ImVec2(480, 640));
    if (ImGui::BeginPopupModal("ProjectSelector",
                               nullptr,
                               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) {
      ImGui::Text("Recent Projects");
      const float x = ImGui::GetContentRegionAvail().x;
      const float y = ImGui::GetFrameHeight();
      for (auto& project : EditorConfig::Get()->GetRecentProjects()) {
        if (ImGui::Button(std::filesystem::path(project).filename().string().c_str(), {x, y})) {
          LoadProjectForEditor(project);
        }
      }

      ImGui::Separator();
      if (ImGui::Button(StringUtils::FromChar8T(ICON_MDI_FILE_DOCUMENT " New Project"), {x, y})) {
        Project::New();
        Visible = false;
      }
      ImGui::SetNextItemWidth(x);
      if (ImGui::Button(StringUtils::FromChar8T(ICON_MDI_UPLOAD " Load Project"), {x, y})) {
        const std::string filepath = FileDialogs::OpenFile({{"Oxylus Project", "oxproj"}});
        if (!filepath.empty()) {
          LoadProjectForEditor(filepath);
        }
      }

      ImGui::EndPopup();
    }
  }
}
