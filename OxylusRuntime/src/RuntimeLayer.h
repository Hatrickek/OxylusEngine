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
    void OnAttach(Oxylus::EventDispatcher& dispatcher) override;
    void OnDetach() override;
    void OnUpdate(float deltaTime) override;
    void OnImGuiRender() override;

    static RuntimeLayer* Get() { return s_Instance; }

    static std::string GetAssetsPath(const std::string_view path) {
      return (std::filesystem::path(ProjectPath) / "Assets" / path).string();
    }

  private:
    void LoadScene();
    bool OnSceneReload(Oxylus::ReloadSceneEvent& e);
    void RenderFinalImage() const;

  private:
    Oxylus::EventDispatcher m_Dispatcher;
    static RuntimeLayer* s_Instance;
    Oxylus::Ref<Oxylus::Scene> m_Scene;
    Oxylus::Entity m_CameraEntity;
  };
}
