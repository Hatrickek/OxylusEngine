#pragma once
#include <Core/Layer.h>

#include "Core/Entity.h"
#include "Core/Systems/HotReloadableScenes.h"

#include "Scene/Scene.h"

constexpr auto ProjectPath = "SandboxProject";

namespace OxylusRuntime {
  class RuntimeLayer : public Oxylus::Layer {
  public:
    RuntimeLayer();
    ~RuntimeLayer() override;
    void on_attach(Oxylus::EventDispatcher& dispatcher) override;
    void on_detach() override;
    void on_update(const Oxylus::Timestep& deltaTime) override;
    void on_imgui_render() override;

    static RuntimeLayer* Get() { return s_Instance; }

    static std::string get_assets_path(const std::string_view path) {
      return (std::filesystem::path(ProjectPath) / "Assets" / path).string();
    }

  private:
    void LoadScene();
    bool OnSceneReload(Oxylus::ReloadSceneEvent& e);

  private:
    Oxylus::EventDispatcher m_Dispatcher;
    static RuntimeLayer* s_Instance;
    Oxylus::Ref<Oxylus::Scene> scene;
    Oxylus::Entity m_CameraEntity;
  };
}
