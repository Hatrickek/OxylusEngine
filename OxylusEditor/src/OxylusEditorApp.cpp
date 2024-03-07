#include "Core/EntryPoint.h"
#include "EditorLayer.h"
#include "Core/Project.h"

namespace ox {
class OxylusEditor : public App {
public:
  OxylusEditor(const AppSpec& spec) : App(spec) { }
};

App* create_application(ApplicationCommandLineArgs args) {
  AppSpec spec;
#ifdef OX_RELEASE
    spec.name = "Oxylus Engine - Editor (Vulkan) - Release";
#endif
#ifdef OX_DEBUG
  spec.name = "Oxylus Engine - Editor (Vulkan) - Debug";
#endif
#ifdef OX_DISTRIBUTION
    spec.name = "Oxylus Engine - Editor (Vulkan) - Dist";
#endif
  spec.working_directory = std::filesystem::current_path().string();
  spec.command_line_args = args;

  const auto app = new OxylusEditor(spec);

  app->push_layer(new EditorLayer());

  return app;
}
}
