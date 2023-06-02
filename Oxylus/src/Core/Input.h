#pragma once

#include <functional>
#include "Keycodes.h"
#include <GLFW/glfw3.h>
#include "glm/vec2.hpp"

namespace Oxylus {
  class Input {
  public:
    enum class CursorState {
      // Locks and hides the cursor.
      DISABLED = GLFW_CURSOR_DISABLED,
      // Normal cursor state.
      NORMAL = GLFW_CURSOR_NORMAL,
      // Just hides the cursor.
      HIDDEN = GLFW_CURSOR_HIDDEN
    };

    static void Init();

    static bool GetKey(KeyCode key);
    static bool GetKeyDown(KeyCode key);
    static bool GetKeyUp(KeyCode key);

    /// Mouse
    static bool GetMouseButtonDown(MouseCode button);
    static bool IsMouseDoubleClicked(MouseCode button);
    static glm::vec2 GetMousePosition();
    static float GetMouseOffsetX();
    static float GetMouseOffsetY();
    static float GetMouseScrollOffsetY();

    /// Cursor
    static CursorState GetCursorState();
    static void SetCursorState(CursorState state, GLFWwindow* window);
    static GLFWcursor* LoadCursorIcon(const char* imagePath);
    /// \param cursor https://www.glfw.org/docs/3.4/group__shapes.html
    static GLFWcursor* LoadCursorIconStandard(int cursor);
    static void SetCursorIcon(GLFWcursor* cursor);
    static void SetCursorIconDefault();
    static void SetCursorPosition(float X, float Y);
    static void DestroyCursor(GLFWcursor* cursor);

    /// Register an event which will be fired when a mouse event happens.
    static void RegisterMouseEvent(const std::function<void()>& event);

    /// Register an event which will be fired when a keyboard event happens.
    static void RegisterKeyboardEvent(const std::function<void()>& event);

    /// Register an event which will be fired when a mouse event happens and will be destroyed afterwards.
    static void RegisterMouseEventOnce(const std::function<void()>& event);

    // Register an event which will be fired when a keyboard event happens and will be destoyed afterwards.
    static void RegisterKeyboardEventOnce(const std::function<void()>& event);

  private:
    static struct Events {
      std::vector<std::function<void()>> RegisteredMouseEvents;
      std::vector<std::function<void()>> RegisteredMouseEventsOnce;
      std::vector<std::function<void()>> RegisteredKeyboardEvents;
      std::vector<std::function<void()>> RegisteredKeyboardEventsOnce;
    } s_Events;

    static void MouseCallback(GLFWwindow* window, double xposIn, double yposIn);

    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
  };
}
