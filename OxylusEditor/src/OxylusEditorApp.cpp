#include "Core/EntryPoint.h"
#include "EditorLayer.h"
#include "Core/Project.h"

namespace Oxylus {
  class MavreasEditor : public Application {
  public:
    MavreasEditor(const AppSpec& spec) : Application(spec) { }
  };

  Application* create_application(ApplicationCommandLineArgs args) {
    AppSpec spec;
#ifdef OX_RELEASE
    spec.Name = "Oxylus Engine - Editor (Vulkan) - Release";
#endif
#ifdef OX_DEBUG
    spec.name = "Oxylus Engine - Editor (Vulkan) - Debug";
#endif
#ifdef OX_DISTRIBUTION
    spec.Name = "Oxylus Engine - Editor (Vulkan) - Dist";
#endif
    spec.working_directory = std::filesystem::current_path().string();
    spec.command_line_args = args;
    spec.custom_window_title = false;

    const auto app = new MavreasEditor(spec);

    app->push_layer(new EditorLayer());

    return app;
  }
}
