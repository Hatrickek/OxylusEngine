#include "Input.h"
#include "Render/Window.h"
#include "stb_image.h"

namespace Oxylus {
void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);

static float s_MouseOffsetX;
static float s_MouseOffsetY;
static float s_MouseScrollOffsetY;
static glm::vec2 s_MousePos;
Input::CursorState cursorState = Input::CursorState::DISABLED;

void Input::init() {
  glfwSetCursorPosCallback(Window::get_glfw_window(), MouseCallback);
  glfwSetScrollCallback(Window::get_glfw_window(), ScrollCallback);
  glfwSetKeyCallback(Window::get_glfw_window(), KeyCallback);
}

bool Input::GetKey(const KeyCode key) {
  const auto state = glfwGetKey(Window::get_glfw_window(), key);
  return state == GLFW_PRESS || state == GLFW_REPEAT;
}

bool Input::GetKeyDown(const KeyCode key) {
  return glfwGetKey(Window::get_glfw_window(), key) == GLFW_PRESS;
}

bool Input::GetKeyUp(const KeyCode key) {
  const auto state = glfwGetKey(Window::get_glfw_window(), key);
  return state == GLFW_RELEASE;
}

bool Input::IsMouseDoubleClicked(const MouseCode button) {
  return ImGui::IsMouseDoubleClicked(button);
}

bool Input::GetMouseButtonDown(const MouseCode button) {
  const auto state = glfwGetMouseButton(Window::get_glfw_window(), button);
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
  GLFWwindow* window = Window::get_glfw_window();
  glfwSetCursorPos(window, X, Y);
}

Input::CursorState Input::GetCursorState() {
  return cursorState;
}

GLFWcursor* Input::LoadCursorIcon(const char* imagePath) {
  int width, height, channels = 4;
  const auto imageData = stbi_load(imagePath, &width, &height, &channels, STBI_rgb_alpha);
  const GLFWimage* cursorImage = new GLFWimage{.width = width, .height = height, .pixels = imageData};
  const auto cursor = glfwCreateCursor(cursorImage, 0, 0);
  stbi_image_free(imageData);

  return cursor;
}

GLFWcursor* Input::LoadCursorIconStandard(const int cursor) {
  return glfwCreateStandardCursor(cursor);
}

void Input::SetCursorIcon(GLFWcursor* cursor) {
  glfwSetCursor(Window::get_glfw_window(), cursor);
}

void Input::SetCursorIconDefault() {
  glfwSetCursor(Window::get_glfw_window(), nullptr);
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

void Input::DestroyCursor(GLFWcursor* cursor) {
  glfwDestroyCursor(cursor);
}

void Input::MouseCallback(GLFWwindow* window, const double xposIn, const double yposIn) {
  s_MouseOffsetX = s_MousePos.x - static_cast<float>(xposIn);
  s_MouseOffsetY = s_MousePos.y - static_cast<float>(yposIn);
  s_MousePos = glm::vec2{static_cast<float>(xposIn), static_cast<float>(yposIn)};
}

void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
  s_MouseScrollOffsetY = (float)yoffset;
}

void Input::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {}
}
