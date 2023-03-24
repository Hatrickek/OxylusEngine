#include <OxylusEngine.h>
#include <core/EntryPoint.h>
#include "EditorLayer.h"
#include "Core/Project.h"
#include "Core/Systems/HotReloadableScenes.h"

namespace Oxylus {
  class MavreasEditor : public Application {
  public:
    MavreasEditor(const AppSpec& spec) : Application(spec) { }
  };

  Application* CreateApplication(ApplicationCommandLineArgs args) {
    AppSpec spec;
#ifdef OX_RELEASE
    spec.Name = "Oxylus Engine - Editor (Vulkan) - Release";
#endif
#ifdef OX_DEBUG
    spec.Name = "Oxylus Engine - Editor (Vulkan) - Debug";
#endif
#ifdef OX_DIST
    spec.Name = "Oxylus Engine - Editor (Vulkan) - Dist";
#endif
    spec.Backend = Core::RenderBackend::Vulkan;
    spec.WorkingDirectory = std::filesystem::current_path().string();
    spec.CommandLineArgs = args;
    spec.CustomWindowTitle = true;
    spec.UseImGui = true;

    const auto app = new MavreasEditor(spec);

    app->PushLayer(new EditorLayer());

    return app;
  }
}
