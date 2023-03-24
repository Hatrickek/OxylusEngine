#pragma once

#include <filesystem>
#include <functional>

#include "EditorPanel.h"
#include "Render/Vulkan/VulkanImage.h"

#include <imgui.h>

#include <Assets/Assets.h>

namespace Oxylus {
  class AssetsPanel : public EditorPanel {
  public:
    struct Events {
      std::vector<std::function<void()>> OnAssetSelectEvent;
    } Events;

    AssetsPanel();

    void OnImGuiRender() override;

    EditorAsset GetSelectedAsset() const;

  private:
    void RefreshAssetItems(const std::filesystem::path& currentDir, const std::filesystem::path& assetPath);
    void DrawContextMenuItems(const std::filesystem::path& absolutePath, const std::filesystem::path& relativePath,
                              bool isDir) const;
    bool DragDropTarget(const std::filesystem::path& dropPath);

    struct AssetItem {
      std::filesystem::path DirectoryPath;
      std::filesystem::path RelativePath;
      std::filesystem::path AbsolutePath;
      std::string Name = "filename";
      bool isDirectory = false;
    };

    std::filesystem::path m_CurrentDirectory;

    std::vector<AssetItem> m_PanelItems;
    EditorAsset m_SelectedAsset{};

    VulkanImage m_FileImage;
    VulkanImage m_FolderImage;
    VulkanImage m_MeshFileImage;

    ImGuiTextFilter m_Filter;
  };
}
