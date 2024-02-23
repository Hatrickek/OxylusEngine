#include "ProjectPanel.h"

#include <icons/IconsMaterialDesignIcons.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "EditorLayer.h"
#include "Core/Project.h"

#include "UI/OxUI.h"
#include "Utils/EditorConfig.h"
#include "Utils/StringUtils.h"
#include "Utils/FileDialogs.h"

namespace Ox {
ProjectPanel::ProjectPanel() : EditorPanel("Projects", ICON_MDI_ACCOUNT_BADGE, true) { }

void ProjectPanel::on_update() { }

void ProjectPanel::load_project_for_editor(const std::string& filepath) {
  if (Project::load(filepath)) {
    const auto startScene = App::get_absolute(Project::get_active()->get_config().start_scene);
    EditorLayer::get()->open_scene(startScene);
    EditorConfig::get()->add_recent_project(Project::get_active().get());
    EditorLayer::get()->content_panel.invalidate();
    Visible = false;
  }
}

void ProjectPanel::new_project(const std::string& project_dir, const std::string& project_name, const std::string& project_asset_dir) {
  const auto project = Project::new_project(project_dir, project_name, project_asset_dir);

  if (project)
    EditorConfig::get()->add_recent_project(project.get());
}

void ProjectPanel::on_imgui_render() {
  if (Visible && !ImGui::IsPopupOpen("ProjectSelector"))
    ImGui::OpenPopup("ProjectSelector");
  constexpr auto flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings
                         | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking;
  static bool draw_new_project_panel = false;

  OxUI::center_next_window();
  if (ImGui::BeginPopupModal("ProjectSelector", nullptr, flags)) {
    const float x = ImGui::GetContentRegionAvail().x;
    const float y = ImGui::GetFrameHeight();

    const auto banner_size = EditorLayer::get()->engine_banner->get_texture().extent;

    const auto scale = Window::get_content_scale();

    OxUI::image(EditorLayer::get()->engine_banner->get_texture(), {(float)banner_size.width * scale.x, (float)banner_size.height * scale.y});
    OxUI::spacing(2);
    ImGui::SeparatorText("Projects");
    OxUI::spacing(2);

    if (draw_new_project_panel) {
      static std::string project_dir = {};
      static std::string project_name = "New Project";
      static std::string project_asset_dir = "Assets";
      OxUI::begin_properties();
      {
        OxUI::begin_property_grid("Directory", nullptr, false);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.8f);
        ImGui::InputText("##Directory", &project_dir, flags);
        ImGui::SameLine();
        if (ImGui::Button(StringUtils::from_char8_t(ICON_MDI_FOLDER), {ImGui::GetContentRegionAvail().x, 0})) {
          project_dir = App::get_system<FileDialogs>()->open_dir();
          project_dir = FileSystem::append_paths(project_dir, project_name);
        }
        OxUI::end_property_grid();
      }
      OxUI::property("Name", &project_name);
      OxUI::property("Asset Directory", &project_asset_dir);
      OxUI::end_properties();
      ImGui::Separator();
      ImGui::SetNextItemWidth(-1);
      if (ImGui::Button("Create", ImVec2(120, 0))) {
        new_project(project_dir, project_name, project_asset_dir);
        Visible = false;
        ImGui::CloseCurrentPopup();
      }
      ImGui::SetItemDefaultFocus();
      ImGui::SameLine();
      if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        draw_new_project_panel = false;
      }
    }
    else {
      const auto projects = EditorConfig::get()->get_recent_projects();
      for (auto& project : projects) {
        auto project_name = FileSystem::get_file_name(project);
        if (ImGui::Button(project_name.c_str(), {x, y})) {
          load_project_for_editor(project);
        }
      }

      ImGui::Separator();
      if (ImGui::Button(StringUtils::from_char8_t(ICON_MDI_FILE_DOCUMENT " New Project"), {x, y})) {
        draw_new_project_panel = true;
      }
      ImGui::SetNextItemWidth(x);
      if (ImGui::Button(StringUtils::from_char8_t(ICON_MDI_UPLOAD " Load Project"), {x, y})) {
        const std::string filepath = App::get_system<FileDialogs>()->open_file({{"Oxylus Project", "oxproj"}});
        if (!filepath.empty()) {
          load_project_for_editor(filepath);
        }
      }
    }
    OxUI::spacing(4);
    ImGui::EndPopup();
  }
}
}
