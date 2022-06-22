#include "window.h"

#include "log.h"

namespace FlatEngine {
	unsigned int Window::SCR_WIDTH = 1600;
	unsigned int Window::SCR_HEIGHT = 900;
	GLFWwindow* Window::window;
	void Window::InitOpenGLWindow() {
		glfwInit();
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_SAMPLES, 4);
		// glfw window creation
		window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "FlatEngine", NULL, NULL);
		if (window == NULL) {
			FE_LOG_ERROR("Failed to create GLFW window");
			glfwTerminate();
			ExitWindow(window);
		}
		glfwMakeContextCurrent(window);

		Input::SetCursorState(Input::CURSOR_STATE_DISABLED, window);

		// glad: load all OpenGL function pointers
		if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
			FE_LOG_ERROR("Failed to initialize GLAD");
			ExitWindow(window);
		}
	}
	GLFWwindow* Window::GetOpenGLWindow() {
		return window;
	}
	glm::vec2 Window::GetWindowSize() {
		glm::vec2 size;
		size.x = static_cast<float>(SCR_WIDTH);
		size.y = static_cast<float>(SCR_HEIGHT);
		return size;
	}
	void Window::ExitWindow(GLFWwindow* m_window) {
		glfwSetWindowShouldClose(m_window, true);
	}
}
