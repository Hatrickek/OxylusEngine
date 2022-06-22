#pragma once
#include <GLFW/glfw3.h>

namespace FlatEngine{
	class UI {
	public:
		//imgui calls
		static void InitUI(GLFWwindow* window);
		static void RenderImgui();
		static void UpdateImgui();
		static void EndImgui();

		//Custom windows
		static void DrawEditorUI();
		static void DrawImguiPerformanceOverlay();
		static void ShowImguiDockSpace();
		static void DrawSceneHierarchy();
		static void DrawConsoleWindow();
		static void DrawDebugPanel();
		static void DrawGizmos(); 
	};
}
