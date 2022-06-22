#pragma once
#include <GLFW/glfw3.h>
#include "keycodes.h"
#include "glm/vec2.hpp"

namespace FlatEngine {
	class Input {
	public:
		enum CursorState {
			CURSOR_STATE_DISABLED = GLFW_CURSOR_DISABLED,	// Locks and hides the cursor.
			CURSOR_STATE_NORMAL	 = GLFW_CURSOR_NORMAL,		// Normal cursor state.
			CURSOR_STATE_HIDDEN   = GLFW_CURSOR_HIDDEN		// Just hides the cursor.
		};
		static bool GetKey(KeyCode key);
		static bool GetKeyDown(KeyCode key);
		static bool GetKeyUp(KeyCode key);
		static bool GetMouseButtonDown(MouseCode button);
		static glm::vec2 GetMousePosition();
		static void SetCursorPosition(float X, float Y);
		static void SetCursorState(CursorState state, GLFWwindow* window);
		static CursorState GetCursorState();
	};
}
