#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include "input.h"
namespace FlatEngine {
	class Window {
	public:
		static unsigned int SCR_WIDTH;
		static unsigned int SCR_HEIGHT;

		static void InitOpenGLWindow();
		static GLFWwindow* GetOpenGLWindow();
		static glm::vec2 GetWindowSize();
		static void ExitWindow(GLFWwindow* m_window);
		
	private:
		static GLFWwindow* window;
	};
}