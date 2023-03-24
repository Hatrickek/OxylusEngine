#pragma once

#include <filesystem>
#include <string>

namespace Oxylus {
  class EditorConfig {
  public:
    EditorConfig();
    ~EditorConfig() = default;

    static EditorConfig* Get() { return s_Instace; }

    void LoadConfig();
    void SaveConfig() const;

    void AddRecentProject(const std::string& path);

    const std::vector<std::string>& GetRecentProjects() const { return m_RecentProjects; }

  private:
    std::vector<std::string> m_RecentProjects{};
    static EditorConfig* s_Instace;
  };
}
