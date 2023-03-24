#pragma once
#include <filesystem>
#include <string>

namespace Oxylus {
  struct ProjectConfig {
    std::string Name = "Untitled";

    std::string StartScene;
    std::string AssetDirectory;
  };

  class Project {
  public:
    ProjectConfig& GetConfig() {
      return m_Config;
    }

    static std::filesystem::path GetProjectDirectory() {
      return s_ActiveProject->m_ProjectDirectory;
    }

    static std::filesystem::path GetAssetDirectory() {
      return GetProjectDirectory() / s_ActiveProject->m_Config.AssetDirectory;
    }

    static Ref<Project> GetActive() {
      return s_ActiveProject;
    }

    static Ref<Project> New();
    static Ref<Project> Load(const std::filesystem::path& path);
    static bool SaveActive(const std::filesystem::path& path);

  private:
    ProjectConfig m_Config;
    std::filesystem::path m_ProjectDirectory;
    inline static Ref<Project> s_ActiveProject;
  };
}
