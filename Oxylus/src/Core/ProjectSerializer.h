#pragma once
#include "Project.h"

namespace Oxylus {
  class ProjectSerializer {
  public:
    ProjectSerializer(Ref<Project> project);

    bool Serialize(const std::filesystem::path& filePath) const;
    bool Deserialize(const std::filesystem::path& filePath) const;

  private:
    Ref<Project> m_Project;
  };
}
