#include "Project.h"

#include "Base.h"
#include "ProjectSerializer.h"
#include "Utils/Log.h"

namespace Oxylus {
Ref<Project> Project::New() {
  s_ActiveProject = CreateRef<Project>();
  return s_ActiveProject;
}

Ref<Project> Project::Load(const std::filesystem::path& path) {
  const Ref<Project> project = CreateRef<Project>();

  const ProjectSerializer serializer(project);
  if (serializer.Deserialize(path)) {
    project->m_ProjectDirectory = path.parent_path();
    s_ActiveProject = project;
    OX_CORE_INFO("Project loaded: {0}", project->GetConfig().Name);
    return s_ActiveProject;
  }

  return nullptr;
}

bool Project::SaveActive(const std::filesystem::path& path) {
  const ProjectSerializer serializer(s_ActiveProject);
  if (serializer.Serialize(path)) {
    s_ActiveProject->m_ProjectDirectory = path.parent_path();
    return true;
  }
  return false;
}
}
