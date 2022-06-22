#pragma once
#include "core/entity.h"

namespace FlatEngine {
	class SceneHPanel {
	public:
		static bool scenehierarchy_window;

		static void DrawPanel();
		static void DrawInspectorPanel();
		static void DrawEntityNode(Entity entity);
		static void SetScene(const Ref<Scene>& scene);
		static Entity GetSelectedEntity();
	private:
		static void DrawComponents(Entity entity);
		static Entity m_SelectionContext;
		static Ref<Scene> m_Scene;
	};
}

