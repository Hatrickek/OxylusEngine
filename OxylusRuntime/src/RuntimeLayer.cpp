#include "RuntimeLayer.h"

#include <imgui.h>
#include <Assets/AssetManager.h>
#include <Render/Vulkan/VulkanRenderer.h>
#include <Scene/SceneSerializer.h>
#include <Utils/ImGuiScoped.h>
#include <entt.hpp>

#include "Systems/FreeCamera.h"

namespace OxylusRuntime {
  using namespace Oxylus;
  RuntimeLayer* RuntimeLayer::s_Instance = nullptr;

  RuntimeLayer::RuntimeLayer() : Layer("Game Layer") {
    s_Instance = this;
  }

  RuntimeLayer::~RuntimeLayer() = default;

  void RuntimeLayer::OnAttach(EventDispatcher& dispatcher) {
    dispatcher.sink<ReloadSceneEvent>().connect<&RuntimeLayer::OnSceneReload>(*this);
    LoadScene();
  }

  void RuntimeLayer::OnDetach() { }

  void RuntimeLayer::OnUpdate(float deltaTime) {
    m_Scene->OnUpdate(deltaTime);
  }

  void RuntimeLayer::OnImGuiRender() {
    RenderFinalImage();
  }

  void RuntimeLayer::LoadScene() {
    m_Scene = CreateRef<Scene>();
    const SceneSerializer serializer(m_Scene);
    serializer.Deserialize(GetAssetsPath("Scenes/Main.oxscene"));

    m_Scene->AddSystem<FreeCamera>();
  }

  bool RuntimeLayer::OnSceneReload(ReloadSceneEvent&) {
    LoadScene();
    OX_CORE_INFO("Scene reloaded.");
    return true;
  }

  /*TODO(hatrickek): Proper way to render the final image internally as fullscreen without needing this.
  This a "hack" to render the final image as a fullscreen image.
  Currently the final image in engine renderer is rendered to an offscreen framebuffer image.*/
  void RuntimeLayer::RenderFinalImage() const {
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing
                                    | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGuiScoped::StyleVar style(ImGuiStyleVar_WindowPadding, ImVec2{});
    if (ImGui::Begin("FinalImage", nullptr, flags)) {
      ImGui::Image(m_Scene->GetRenderer().GetRenderPipeline()->GetFinalImage().GetDescriptorSet(),
        ImVec2{(float)Window::GetWidth(), (float)Window::GetHeight()});
      ImGui::End();
    }
  }
}
