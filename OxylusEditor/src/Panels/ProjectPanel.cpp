#include "ProjectPanel.h"

#include <icons/IconsMaterialDesignIcons.h>
#include <imgui.h>

#include "EditorLayer.h"
#include "Assets/AssetManager.h"
#include "Core/Project.h"
#include "UI/OxUI.h"
#include "UI/OxUI.h"
#include "Utils/EditorConfig.h"
#include "Utils/StringUtils.h"
#include "Utils/UIUtils.h"

namespace Oxylus {
ProjectPanel::ProjectPanel() : EditorPanel("Projects", ICON_MDI_ACCOUNT_BADGE, true) { }

void ProjectPanel::on_update() { }

void ProjectPanel::LoadProjectForEditor(const std::string& filepath) {
  if (Project::load(filepath)) {
    const auto startScene = AssetManager::get_asset_file_system_path(Project::get_active()->get_config().start_scene);
    EditorLayer::get()->open_scene(startScene);
    EditorConfig::Get()->AddRecentProject(filepath);
    Visible = false;
  }
}

void ProjectPanel::on_imgui_render() {
  if (Visible && !ImGui::IsPopupOpen("ProjectSelector"))
    ImGui::OpenPopup("ProjectSelector");
  ImGui::SetNextWindowSize(ImVec2(480, 320), ImGuiCond_FirstUseEver);
  OxUI::CenterNextWindow();
  constexpr auto flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings
                         | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking;
  if (ImGui::BeginPopupModal("ProjectSelector", nullptr, flags)) {
    const float x = ImGui::GetContentRegionAvail().x;
    const float y = ImGui::GetFrameHeight();

    OxUI::image(EditorLayer::get()->engine_banner->get_texture(), {x, 111});

    ImGui::Text("Recent Projects");
    for (auto& project : EditorConfig::Get()->GetRecentProjects()) {
      if (ImGui::Button(std::filesystem::path(project).filename().string().c_str(), {x, y})) {
        LoadProjectForEditor(project);
      }
    }

    ImGui::Separator();
    if (ImGui::Button(StringUtils::from_char8_t(ICON_MDI_FILE_DOCUMENT " New Project"), {x, y})) {
      Project::create_new();
      Visible = false;
    }
    ImGui::SetNextItemWidth(x);
    if (ImGui::Button(StringUtils::from_char8_t(ICON_MDI_UPLOAD " Load Project"), {x, y})) {
      const std::string filepath = FileDialogs::open_file({{"Oxylus Project", "oxproj"}});
      if (!filepath.empty()) {
        LoadProjectForEditor(filepath);
      }
    }
  }
  ImGui::EndPopup();
}
}
