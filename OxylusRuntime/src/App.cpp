#include <Core/EntryPoint.h>
#include <Core/Systems/HotReloadableScenes.h>

#include "RuntimeLayer.h"
#include "Core/Systems/SystemManager.h"

namespace Oxylus {
  class OxylusRuntime : public Application {
  public:
    OxylusRuntime(const AppSpec& spec) : Application(spec) { }
  };

  Application* create_application(const ApplicationCommandLineArgs args) {
    AppSpec spec;
    spec.name = "OxylusRuntime";
    spec.working_directory = std::filesystem::current_path().string();
    spec.command_line_args = args;
    spec.custom_window_title = false;

    const auto app = new OxylusRuntime(spec);
    app->push_layer(new ::OxylusRuntime::RuntimeLayer());
    app->get_system_manager()
        ->AddSystem<HotReloadableScenes>(::OxylusRuntime::RuntimeLayer::GetAssetsPath("Scenes/Main.oxscene"));

    return app;
  }
}
