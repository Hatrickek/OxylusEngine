#pragma once

#include "Project.h"
#include "Base.h"

namespace Oxylus {
class ProjectSerializer {
public:
  ProjectSerializer(Ref<Project> project);

  bool serialize(const std::filesystem::path& file_path) const;
  bool deserialize(const std::filesystem::path& file_path) const;

private:
  Ref<Project> m_project;
};
}
