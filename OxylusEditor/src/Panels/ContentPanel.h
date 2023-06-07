#pragma once

#include <filesystem>
#include <stack>

#include "EditorPanel.h"
#include <imgui.h>
#include "Render/Vulkan/VulkanImage.h"

namespace Oxylus {
  enum class FileType {
    Unknown = 0,
    Scene,
    Prefab,
    Shader,
    Texture,
    Cubemap,
    Model,
    Audio,
    Material
  };

  class ContentPanel : public EditorPanel {
  public:
    explicit ContentPanel(const char* name = "Assets");

    ~ContentPanel() override = default;

    ContentPanel(const ContentPanel& other) = delete;
    ContentPanel(ContentPanel&& other) = delete;
    ContentPanel& operator=(const ContentPanel& other) = delete;
    ContentPanel& operator=(ContentPanel&& other) = delete;

    void Init();
    void OnUpdate() override;
    void OnImGuiRender() override;

    void Invalidate();

  private:
    std::pair<bool, uint32_t> DirectoryTreeViewRecursive(const std::filesystem::path& path, uint32_t* count, int* selectionMask, ImGuiTreeNodeFlags flags);
    void RenderHeader();
    void RenderSideView();
    void RenderBody(bool grid);
    void UpdateDirectoryEntries(const std::filesystem::path& directory);
    void Refresh() { UpdateDirectoryEntries(m_CurrentDirectory); }

    void DrawContextMenuItems(const std::filesystem::path& context, bool isDir);

  private:
    struct File {
      std::string Name;
      std::string Filepath;
      std::string Extension;
      std::filesystem::directory_entry DirectoryEntry;
      Ref<VulkanImage> Thumbnail = nullptr;
      bool IsDirectory = false;

      FileType Type;
      std::string_view FileTypeString;
      ImVec4 FileTypeIndicatorColor;
    };

    std::filesystem::path m_AssetsDirectory;
    std::filesystem::path m_CurrentDirectory;
    std::stack<std::filesystem::path> m_BackStack;
    std::vector<File> m_DirectoryEntries;
    uint32_t m_CurrentlyVisibleItemsTreeView = 0;
    float m_ThumbnailSize = 128.0f;
    ImGuiTextFilter m_Filter;
    float m_ElapsedTime = 0.0f;
    bool m_TexturePreviews = false;

    Ref<VulkanImage> m_WhiteTexture;
    Ref<VulkanImage> m_DirectoryIcon;
    Ref<VulkanImage> m_MeshIcon;
    Ref<VulkanImage> m_FileIcon;
    std::filesystem::path m_DirectoryToDelete;
  };
}
