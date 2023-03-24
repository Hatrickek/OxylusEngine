#include "src/oxpch.h"
#include "Input.h"
#include "Render/Window.h"
#include "Utils/Timestep.h"

namespace Oxylus {
  void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);

  Input::Events Input::s_Events;
  static float s_MouseOffsetX;
  static float s_MouseOffsetY;
  static float s_MouseScrollOffsetY;
  int leftMouseClickedCount;
  int rightMouseClickedCount;
  static glm::vec2 s_MousePos;
  Input::CursorState cursorState = Input::CursorState::DISABLED;

  void Input::Init() {
    glfwSetCursorPosCallback(Window::GetGLFWWindow(), MouseCallback);
    glfwSetScrollCallback(Window::GetGLFWWindow(), ScrollCallback);
    glfwSetKeyCallback(Window::GetGLFWWindow(), KeyCallback);
  }

  bool Input::GetKey(const KeyCode key) {
    const auto state = glfwGetKey(Window::GetGLFWWindow(), key);
    return state == GLFW_PRESS || state == GLFW_REPEAT;
  }

  bool Input::GetKeyDown(const KeyCode key) {
    static bool s_Pressed = false;
    static bool s_CurrentlyPressed = false;
    s_CurrentlyPressed = glfwGetKey(Window::GetGLFWWindow(), key) == GLFW_PRESS;
    if (s_CurrentlyPressed && !s_Pressed) {
      return true;
    }
    s_Pressed = s_CurrentlyPressed;
    return false;
  }

  bool Input::GetKeyUp(const KeyCode key) {
    const auto state = glfwGetKey(Window::GetGLFWWindow(), key);
    return state == GLFW_RELEASE;
  }

  bool Input::IsMouseDoubleClicked(const MouseCode button) {
    return ImGui::IsMouseDoubleClicked(button);
  }

  bool Input::GetMouseButtonDown(const MouseCode button) {
    const auto state = glfwGetMouseButton(Window::GetGLFWWindow(), button);
    return state == GLFW_PRESS;
  }

  glm::vec2 Input::GetMousePosition() {
    return s_MousePos;
  }

  float Input::GetMouseOffsetX() {
    return s_MouseOffsetX;
  }

  float Input::GetMouseOffsetY() {
    return s_MouseOffsetY;
  }

  float Input::GetMouseScrollOffsetY() {
    return s_MouseScrollOffsetY;
  }

  void Input::SetCursorPosition(const float X, const float Y) {
    GLFWwindow* window = Window::GetGLFWWindow();
    glfwSetCursorPos(window, X, Y);
  }

  Input::CursorState Input::GetCursorState() {
    return cursorState;
  }

  void Input::RegisterMouseEvent(const std::function<void()>& event) {
    s_Events.RegisteredMouseEvents.emplace_back(event);
  }

  void Input::RegisterKeyboardEvent(const std::function<void()>& event) {
    s_Events.RegisteredKeyboardEvents.emplace_back(event);
  }

  void Input::RegisterMouseEventOnce(const std::function<void()>& event) {
    s_Events.RegisteredMouseEventsOnce.emplace_back(event);
  }

  void Input::RegisterKeyboardEventOnce(const std::function<void()>& event) {
    s_Events.RegisteredKeyboardEventsOnce.emplace_back(event);
  }

  void Input::SetCursorState(const CursorState state, GLFWwindow* window) {
    switch (state) {
      case CursorState::DISABLED: cursorState = CursorState::DISABLED;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        break;
      case CursorState::NORMAL: cursorState = CursorState::NORMAL;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        break;
      case CursorState::HIDDEN: cursorState = CursorState::HIDDEN;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
        break;
    }
  }

  void Input::MouseCallback(GLFWwindow* window, const double xposIn, const double yposIn) {
    s_MouseOffsetX = s_MousePos.x - static_cast<float>(xposIn);
    s_MouseOffsetY = s_MousePos.y - static_cast<float>(yposIn);
    s_MousePos = glm::vec2{static_cast<float>(xposIn), static_cast<float>(yposIn)};
    for (auto& event : s_Events.RegisteredMouseEvents) {
      event();
    }
    for (auto& event : s_Events.RegisteredMouseEventsOnce) {
      event();
    }
    s_Events.RegisteredMouseEventsOnce.clear();
  }

  void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    s_MouseScrollOffsetY = (float)yoffset;
  }

  void Input::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    for (auto& event : s_Events.RegisteredKeyboardEvents) {
      event();
    }
    for (auto& event : s_Events.RegisteredKeyboardEventsOnce) {
      event();
    }
    s_Events.RegisteredKeyboardEventsOnce.clear();
  }
}
