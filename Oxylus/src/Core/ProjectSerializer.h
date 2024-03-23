#pragma once

#include "Project.h"
#include "Base.h"

namespace ox {
class ProjectSerializer {
public:
  ProjectSerializer(Shared<Project> project);

  bool serialize(const std::string& file_path) const;
  bool deserialize(const std::string& file_path) const;

private:
  Shared<Project> project;
};
}
