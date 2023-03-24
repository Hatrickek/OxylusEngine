#include "AssetsPanel.h"

#include <Assets/AssetManager.h>
#include <Assets/MaterialSerializer.h>

#include <icons/IconsMaterialDesignIcons.h>

#include "imgui.h"
#include "Core/Entity.h"
#include "Core/Project.h"
#include "Scene/EntitySerializer.h"
#include "Thread/ThreadManager.h"
#include "UI/IGUI.h"
#include "Utils/UIUtils.h"

namespace Oxylus {
  AssetsPanel::AssetsPanel() : EditorPanel("Assets", ICON_MDI_FOLDER_STAR, true) {
    VulkanImageDescription buttonImageDescripton;
    buttonImageDescripton.Format = vk::Format::eR8G8B8A8Unorm;
    buttonImageDescripton.CreateDescriptorSet = true;

    buttonImageDescripton.Path = "resources/icons/FileIcon.png";
    m_FileImage.Create(buttonImageDescripton);

    buttonImageDescripton.Path = "resources/icons/FolderIcon.png";
    m_FolderImage.Create(buttonImageDescripton);

    buttonImageDescripton.Path = "resources/icons/MeshFileIcon.png";
    m_MeshFileImage.Create(buttonImageDescripton);
  }

  bool AssetsPanel::DragDropTarget(const std::filesystem::path& dropPath) {
    if (ImGui::BeginDragDropTarget()) {
      if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Entity")) {
        const Entity entity = *static_cast<Entity*>(payload->Data);
        const std::filesystem::path path =
                dropPath / std::string(entity.GetComponent<TagComponent>().Tag + ".oxprefab");
        ThreadManager::Get()->AssetThread.QueueJob([path, entity] {
          EntitySerializer::SerializeEntityAsPrefab(path.string().c_str(), entity);
        });
        return true;
      }
      ImGui::EndDragDropTarget();
    }

    return false;
  }

  void AssetsPanel::OnImGuiRender() {
    const std::filesystem::path assetPath = Project::GetAssetDirectory();

    bool shouldUpdate = false;

    if (OnBegin(ImGuiWindowFlags_MenuBar)) {
      if (m_PanelItems.empty())
        shouldUpdate = true;

      if (m_CurrentDirectory.empty()) {
        m_CurrentDirectory = assetPath;
        OnEnd();
        return;
      }

      bool panelSettingsPopup = false;

      constexpr float padding = 16.0f;
      static int32_t thumbnailSize = 64;
      const float cellSize = thumbnailSize + padding;

      if (ImGui::BeginMenuBar()) {
        if (ImGui::MenuItem(StringUtils::FromChar8T(ICON_MDI_ARROW_LEFT))) {
          if (m_CurrentDirectory != assetPath) {
            m_CurrentDirectory = m_CurrentDirectory.parent_path();
            RefreshAssetItems(m_CurrentDirectory, assetPath);
          }
        }
        if (ImGui::MenuItem(StringUtils::FromChar8T(ICON_MDI_REFRESH))) {
          RefreshAssetItems(m_CurrentDirectory, assetPath);
        }
        if (ImGui::MenuItem(m_CurrentDirectory.relative_path().string().c_str())) { }
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 30);
        if (ImGui::MenuItem(StringUtils::FromChar8T(ICON_MDI_COGS))) {
          panelSettingsPopup = true;
        }
        ImGui::EndMenuBar();
      }

      if (panelSettingsPopup)
        ImGui::OpenPopup("Asset Panel Settings");

      if (ImGui::BeginPopup("Asset Panel Settings")) {
        IGUI::BeginProperties();
        IGUI::Property<int32_t>("Thumbnail size", thumbnailSize, 8, 64);
        IGUI::EndProperties();
        ImGui::EndPopup();
      }

      static std::filesystem::path s_DirectoryToDelete = {};

      const float panelWidth = ImGui::GetContentRegionAvail().x;
      int columnCount = (int)(panelWidth / cellSize);
      if (columnCount < 1)
        columnCount = 1;
      ImGui::Columns(columnCount, nullptr, false);

      //TODO: Invisible button for drag and drop
      //const ImVec2 region = ImGui::GetContentRegionAvail();
      //ImGui::InvisibleButton("##DragDropTargetAssetPanelBody", region);

      for (const auto& [directoryPath, relativePath, absolutePath, name, isDirectory] : m_PanelItems) {
        ImGui::PushID(name.c_str());
        auto icon = isDirectory ? m_FolderImage.GetDescriptorSet() : m_FileImage.GetDescriptorSet();
        if (!isDirectory && relativePath.extension().string() == ".gltf") {
          icon = m_MeshFileImage.GetDescriptorSet();
        }
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::ImageButton(icon, {(float)thumbnailSize, (float)thumbnailSize});
        if (ImGui::BeginDragDropSource()) {
          const wchar_t* itemPath = relativePath.c_str();
          ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", itemPath, (wcslen(itemPath) + 1) * sizeof(wchar_t));
          ImGui::EndDragDropSource();
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
          if (isDirectory) {
            m_CurrentDirectory /= directoryPath.filename();
            RefreshAssetItems(m_CurrentDirectory, assetPath);
            ImGui::NextColumn();

            ImGui::PopID();

            m_SelectedAsset = {};

            break;
          }

          //Filter selection by extension
          {
            if (relativePath.extension().string() == ".oxmat") {
              EditorAsset asset;
              asset.Type = AssetType::Material;
              asset.Path = absolutePath.string();
              m_SelectedAsset = asset;
              for (auto& event : Events.OnAssetSelectEvent)
                event();
            }
          }
        }

        if (DragDropTarget(m_CurrentDirectory))
          shouldUpdate = true;

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 8.0f)); //TODO: Get this from editor theme
        if (ImGui::BeginPopupContextItem()) {
          if (ImGui::MenuItem("Delete")) {
            s_DirectoryToDelete = directoryPath;
            ImGui::CloseCurrentPopup();
          }
          if (ImGui::MenuItem("Rename")) {
            ImGui::CloseCurrentPopup();
          }

          ImGui::Separator();

          DrawContextMenuItems(absolutePath.c_str(), m_CurrentDirectory, isDirectory);

          ImGui::EndPopup();
        }
        ImGui::PopStyleVar();

        ImGui::TextWrapped(name.data());

        ImGui::NextColumn();

        ImGui::PopID();
      }

      if (ImGui::BeginPopupContextWindow("AssetPanelHierarchyContextWindow",
                                         ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        DrawContextMenuItems(absolute(m_CurrentDirectory), m_CurrentDirectory, true);
        ImGui::EndPopup();
      }

      if (!s_DirectoryToDelete.empty()) {
        if (!ImGui::IsPopupOpen("Delete?"))
          ImGui::OpenPopup("Delete?");
      }

      if (shouldUpdate) {
        RefreshAssetItems(m_CurrentDirectory, assetPath);
      }

      if (ImGui::BeginPopupModal("Delete?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s will be deleted. Are you sure? \nThis operation cannot be undone!\n\n",
                    s_DirectoryToDelete.string().c_str());
        ImGui::Separator();
        if (ImGui::Button("OK", ImVec2(120, 0))) {
          std::filesystem::remove_all(s_DirectoryToDelete);
          s_DirectoryToDelete.clear();
          RefreshAssetItems(m_CurrentDirectory, assetPath);
          ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
          s_DirectoryToDelete.clear();
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }

      ImGui::Columns(1);

      OnEnd();
    }
  }

  EditorAsset AssetsPanel::GetSelectedAsset() const {
    return m_SelectedAsset;
  }

  void AssetsPanel::RefreshAssetItems(const std::filesystem::path& currentDir, const std::filesystem::path& assetPath) {
    m_PanelItems.clear();
    for (auto& directoryEntry : std::filesystem::directory_iterator(currentDir)) {
      const auto& dirPath = directoryEntry.path();
      auto relativePath = std::filesystem::relative(dirPath, assetPath);
      const std::string filenameString = relativePath.filename().string();

      m_PanelItems.emplace_back(AssetItem{
              dirPath, relativePath, absolute(dirPath), filenameString, directoryEntry.is_directory()
      });
    }
  }

  void AssetsPanel::DrawContextMenuItems(const std::filesystem::path& absolutePath,
                                         const std::filesystem::path& relativePath,
                                         bool isDir) const {
    if (isDir) {
      //Create asset menu
      if (ImGui::BeginMenu("Create")) {
        if (ImGui::MenuItem("Material")) {
          Material material{};
          material.Create();
          const MaterialSerializer serializer(material);
          const auto filePath = AssetManager::GetAssetFileSystemPath(relativePath) / "NewMaterial.oxmat";
          material.Name = StringUtils::GetName(filePath.string());
          serializer.Serialize(filePath.string());
        }
        ImGui::EndMenu();
      }
    }
    if (ImGui::MenuItem("Show in Explorer")) {
      FileDialogs::OpenFolderAndSelectItem(absolutePath.string().c_str());
      ImGui::CloseCurrentPopup();
    }
  }
}
