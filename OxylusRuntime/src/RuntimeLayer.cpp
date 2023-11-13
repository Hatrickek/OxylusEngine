#include "RuntimeLayer.h"

#include <imgui.h>
#include <Assets/AssetManager.h>
#include <Scene/SceneSerializer.h>

#include "CustomRenderPipeline.h"

#include "Core/Application.h"
#include "Render/Window.h"
#include "Systems/CharacterSystem.h"
#include "Systems/FreeCamera.h"

namespace OxylusRuntime {
  using namespace Oxylus;
  RuntimeLayer* RuntimeLayer::s_Instance = nullptr;

  RuntimeLayer::RuntimeLayer() : Layer("Game Layer") {
    s_Instance = this;
  }

  RuntimeLayer::~RuntimeLayer() = default;

  void RuntimeLayer::on_attach(EventDispatcher& dispatcher) {
    // HotReloadableScenesSystem listener
    dispatcher.sink<ReloadSceneEvent>().connect<&RuntimeLayer::on_scene_reload>(*this);
    load_scene();
  }

  void RuntimeLayer::on_detach() { }

  void RuntimeLayer::on_update(const Timestep& deltaTime) {
    scene->on_runtime_update(deltaTime);
  }

  void RuntimeLayer::on_imgui_render() {
    scene->on_imgui_render(Application::get_timestep());
  }

  void RuntimeLayer::load_scene() {
    // Instead of leaving the constructor empty we pass our own custom render pipeline.
    // If left empty the scene will use the DefaultRenderPipeline.
    Ref<CustomRenderPipeline> custom_rp = create_ref<CustomRenderPipeline>();
    scene = create_ref<Scene>(custom_rp);

    const SceneSerializer serializer(scene);
    serializer.deserialize(get_assets_path("Scenes/TestScene.oxscene"));

    scene->on_runtime_start();

    scene->add_system<FreeCamera>();
  }

  bool RuntimeLayer::on_scene_reload(ReloadSceneEvent&) {
    load_scene();
    OX_CORE_INFO("Scene reloaded.");
    return true;
  }
}
