#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Oxylus {
class Project;

class EditorConfig {
  public:
    EditorConfig();
    ~EditorConfig() = default;

    static EditorConfig* get() { return instance; }

    void load_config();
    void save_config() const;

    void add_recent_project(const Project* path);

    const std::vector<std::string>& get_recent_projects() const { return recent_projects; }

  private:
    std::vector<std::string> recent_projects{};
    static EditorConfig* instance;
  };
}
