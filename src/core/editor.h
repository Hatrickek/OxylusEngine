#pragma once
#include "camera.h"
#include "scene.h"

namespace FlatEngine {
	class Editor {
	public:
		static void OnInit();
		static void OnUpdate();
		static Ref<Scene> GetActiveScene();
		static Ref<Camera> GetEditorCamera();
	private:
		static std::shared_ptr<Scene> m_ActiveScene;
		static Ref<Camera> m_EditorCamera;
	};
}
