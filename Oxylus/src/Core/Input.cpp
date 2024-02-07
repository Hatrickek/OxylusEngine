#include "Input.h"
#include "Render/Window.h"
#include "stb_image.h"
#include "Types.h"

namespace Ox {
Input::CursorState Input::cursor_state = CursorState::Disabled;
Input::InputData Input::input_data = {};

void Input::init() {
  glfwSetCursorPosCallback(Window::get_glfw_window(), cursor_pos_callback);
  glfwSetScrollCallback(Window::get_glfw_window(), scroll_callback);
}

void Input::reset_pressed() {
  memset(input_data.key_pressed, 0, MAX_KEYS);
  memset(input_data.m_mouse_clicked, 0, MAX_BUTTONS);
  input_data.scroll_offset_y = 0;
}

void Input::reset() {
  memset(input_data.key_held, 0, MAX_KEYS);
  memset(input_data.key_pressed, 0, MAX_KEYS);
  memset(input_data.m_mouse_clicked, 0, MAX_BUTTONS);
  memset(input_data.m_mouse_held, 0, MAX_BUTTONS);

  input_data.scroll_offset_y = 0;
}

void Input::set_dispatcher_events(EventDispatcher& event_dispatcher) {
  event_dispatcher.sink<KeyPressedEvent>().connect<&Input::on_key_pressed_event>();
  event_dispatcher.sink<KeyReleasedEvent>().connect<&Input::on_key_released_event>();
  event_dispatcher.sink<MouseButtonPressedEvent>().connect<&Input::on_mouse_pressed_event>();
  event_dispatcher.sink<MouseButtonReleasedEvent>().connect<&Input::on_mouse_button_released_event>();
}

void Input::on_key_pressed_event(const KeyPressedEvent& event) {
  set_key_pressed(event.get_key_code(), event.get_repeat_count() < 1);
  set_key_released(event.get_key_code(), false);
  set_key_held(event.get_key_code(), true);
}

void Input::on_key_released_event(const KeyReleasedEvent& event) {
  set_key_pressed(event.get_key_code(), false);
  set_key_released(event.get_key_code(), true);
  set_key_held(event.get_key_code(), false);
}

void Input::on_mouse_pressed_event(const MouseButtonPressedEvent& event) {
  set_mouse_clicked(event.get_mouse_button(), true);
  set_mouse_held(event.get_mouse_button(), true);
}

void Input::on_mouse_button_released_event(const MouseButtonReleasedEvent& event) {
  set_mouse_clicked(event.get_mouse_button(), false);
  set_mouse_held(event.get_mouse_button(), false);
}

Vec2 Input::get_mouse_position() {
  return input_data.mouse_pos;
}

float Input::get_mouse_offset_x() {
  return input_data.mouse_offset_x;
}

float Input::get_mouse_offset_y() {
  return input_data.mouse_offset_y;
}

float Input::get_mouse_scroll_offset_y() {
  return input_data.scroll_offset_y;
}

void Input::set_mouse_position(const float x, const float y) {
  glfwSetCursorPos(Window::get_glfw_window(), x, y);
  input_data.mouse_pos.x = x;
  input_data.mouse_pos.y = y;
}

Input::CursorState Input::get_cursor_state() {
  return cursor_state;
}

GLFWcursor* Input::load_cursor_icon(const char* image_path) {
  int width, height, channels = 4;
  const auto image_data = stbi_load(image_path, &width, &height, &channels, STBI_rgb_alpha);
  const GLFWimage* cursor_image = new GLFWimage{.width = width, .height = height, .pixels = image_data};
  const auto cursor = glfwCreateCursor(cursor_image, 0, 0);
  stbi_image_free(image_data);

  return cursor;
}

GLFWcursor* Input::load_cursor_icon_standard(const int cursor) {
  return glfwCreateStandardCursor(cursor);
}

void Input::set_cursor_icon(GLFWcursor* cursor) {
  glfwSetCursor(Window::get_glfw_window(), cursor);
}

void Input::set_cursor_icon_default() {
  glfwSetCursor(Window::get_glfw_window(), nullptr);
}

void Input::set_cursor_state(const CursorState state) {
  auto window = Window::get_glfw_window();
  switch (state) {
    case CursorState::Disabled: cursor_state = CursorState::Disabled;
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
      break;
    case CursorState::Normal: cursor_state = CursorState::Normal;
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
      break;
    case CursorState::Hidden: cursor_state = CursorState::Hidden;
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
      break;
  }
}

void Input::destroy_cursor(GLFWcursor* cursor) {
  glfwDestroyCursor(cursor);
}

void Input::cursor_pos_callback(GLFWwindow* window, const double xpos_in, const double ypos_in) {
  input_data.mouse_offset_x = input_data.mouse_pos.x - static_cast<float>(xpos_in);
  input_data.mouse_offset_y = input_data.mouse_pos.y - static_cast<float>(ypos_in);
  input_data.mouse_pos = glm::vec2{static_cast<float>(xpos_in), static_cast<float>(ypos_in)};
}

void Input::scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
  input_data.scroll_offset_y = (float)yoffset;
}
}
