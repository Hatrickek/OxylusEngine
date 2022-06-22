#include "input.h"

#include "window.h"


namespace FlatEngine {
	bool Input::GetKey(KeyCode key) {
		GLFWwindow* window = Window::GetOpenGLWindow();
		auto state = glfwGetKey(window, key);
		return state == GLFW_PRESS || state == GLFW_REPEAT;
	}
	bool Input::GetKeyDown(KeyCode key) {
		GLFWwindow* window = Window::GetOpenGLWindow();
		auto state = glfwGetKey(window, key);
		return state == GLFW_PRESS;
	}
	bool Input::GetKeyUp(KeyCode key) {
		GLFWwindow* window = Window::GetOpenGLWindow();
		auto state = glfwGetKey(window, key);
		return state == GLFW_RELEASE;
	}

	bool Input::GetMouseButtonDown(const MouseCode button)
	{
		GLFWwindow* window = Window::GetOpenGLWindow();
		auto state = glfwGetMouseButton(window, button);
		return state == GLFW_PRESS;
	}
	glm::vec2 Input::GetMousePosition()
	{
		GLFWwindow* window = Window::GetOpenGLWindow();
		double xpos, ypos;
		glfwGetCursorPos(window, &xpos, &ypos);
		return { (float)xpos, (float)ypos };
	}
	void Input::SetCursorPosition(float X, float Y) {
		GLFWwindow* window = Window::GetOpenGLWindow();
		glfwSetCursorPos(window, X, Y);
	}
	Input::CursorState cursor_state = Input::CURSOR_STATE_DISABLED;
	Input::CursorState Input::GetCursorState() {
		return cursor_state;
	}
	void Input::SetCursorState(CursorState state, GLFWwindow* window) {
		switch (state) {
		case CURSOR_STATE_DISABLED:
			cursor_state = CURSOR_STATE_DISABLED;
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			break;
		case CURSOR_STATE_NORMAL:
			cursor_state = CURSOR_STATE_NORMAL;
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			break;
		case CURSOR_STATE_HIDDEN:
			cursor_state = CURSOR_STATE_HIDDEN;
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
			break;
		}
	}
}
