#include "editor.h"
#include <core/timestep.h>

#include "entity.h"
#include "components.h"
#include "filesystem.h"
#include "utility.h"
#include "input.h"
#include "window.h"
#include "resources.h"
namespace FlatEngine {
	Ref<Scene> Editor::m_ActiveScene;
	Ref<Camera> Editor::m_EditorCamera;
	void Editor::OnInit() {
		m_EditorCamera = CreateRef<Camera>(glm::vec3(0.0f, 0.0f, 5.0f));

		Resources::InitResources();

		Input::SetCursorState(Input::CURSOR_STATE_NORMAL,Window::GetOpenGLWindow());

		Model patrick_model(FileSystem::getPath("resources/objects/patrick_animated/patrick_animated.obj"));
		Model rsr_model(FileSystem::getPath("resources/objects/911rsr.obj"));

		m_ActiveScene = std::make_shared<Scene>();
		Entity rsr = m_ActiveScene->CreateEntity("RsrModel");
		rsr.GetComponent<TransformComponent>().Scale = glm::vec3(1.5f);
		rsr.GetComponent<TransformComponent>().Translation = glm::vec3(-1.0f, -0.5f, -2.0);
		rsr.AddComponent<MeshRendererComponent>(CreateRef<Model>(rsr_model), Resources::GetDefaultShader(), WHITE);

		Entity patrick = m_ActiveScene->CreateEntity("PatrickModel");
		patrick.GetComponent<TransformComponent>().Scale = glm::vec3(.3f);
		patrick.GetComponent<TransformComponent>().Translation = glm::vec3(2.5f, -0.5f, -3.0);
		patrick.GetComponent<TransformComponent>().Rotation = glm::vec3(0,90,0);
		patrick.AddComponent<MeshRendererComponent>(CreateRef<Model>(patrick_model), Resources::GetDefaultShader(), BLANK);
	}
	void Editor::OnUpdate() {
		m_ActiveScene->OnUpdate(Timestep::GetDeltaTime());
	}
	Ref<Scene> Editor::GetActiveScene() {
		return m_ActiveScene;
	}
	Ref<Camera> Editor::GetEditorCamera() {
		return m_EditorCamera;
	}
}
