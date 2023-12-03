#pragma once

#include <functional>
#include "Keycodes.h"
#include <GLFW/glfw3.h>

#include "ApplicationEvents.h"
#include "Types.h"

#include "Event/Event.h"

#include "glm/vec2.hpp"

namespace Oxylus {
class Input {
public:
  enum class CursorState {
    Disabled = GLFW_CURSOR_DISABLED,
    Normal   = GLFW_CURSOR_NORMAL,
    Hidden   = GLFW_CURSOR_HIDDEN
  };

  static void init();
  static void reset_pressed();
  static void reset();

  static void set_dispatcher_events(EventDispatcher& event_dispatcher);

  static void on_key_pressed_event(const KeyPressedEvent& event);
  static void on_key_released_event(const KeyReleasedEvent& event);
  static void on_mouse_pressed_event(const MouseButtonPressedEvent& event);
  static void on_mouse_button_released_event(const MouseButtonReleasedEvent& event);

  static bool get_key_pressed(const KeyCode key) { return input_data.key_pressed[int(key)]; }
  static bool get_key_released(const KeyCode key) { return input_data.key_released[int(key)]; }
  static bool get_key_held(const KeyCode key) { return input_data.key_held[int(key)]; }
  static bool get_mouse_clicked(const MouseCode key) { return input_data.m_mouse_clicked[int(key)]; }
  static bool get_mouse_held(const MouseCode key) { return input_data.m_mouse_held[int(key)]; }

  /// Mouse
  static Vec2 get_mouse_position();
  static float get_mouse_offset_x();
  static float get_mouse_offset_y();
  static float get_mouse_scroll_offset_y();

  /// Cursor
  static CursorState get_cursor_state();
  static void set_cursor_state(CursorState state);
  static GLFWcursor* load_cursor_icon(const char* image_path);
  /// \param cursor https://www.glfw.org/docs/3.4/group__shapes.html
  static GLFWcursor* load_cursor_icon_standard(int cursor);
  static void set_cursor_icon(GLFWcursor* cursor);
  static void set_cursor_icon_default();
  static void set_cursor_position(float x, float y);
  static void destroy_cursor(GLFWcursor* cursor);

private:
#define MAX_KEYS 128
#define MAX_BUTTONS 32

  static struct InputData {
    bool key_pressed[MAX_KEYS] = {};
    bool key_released[MAX_KEYS] = {};
    bool key_held[MAX_KEYS] = {};
    bool m_mouse_held[MAX_BUTTONS] = {};
    bool m_mouse_clicked[MAX_BUTTONS] = {};

    float mouse_offset_x;
    float mouse_offset_y;
    float scroll_offset_y;
    Vec2 mouse_pos;
  } input_data;

  static CursorState cursor_state;

  static void set_key_pressed(const KeyCode key, const bool a) { input_data.key_pressed[int(key)] = a; }
  static void set_key_released(const KeyCode key, const bool a) { input_data.key_released[int(key)] = a; }
  static void set_key_held(const KeyCode key, const bool a) { input_data.key_held[int(key)] = a; }
  static void set_mouse_clicked(const MouseCode key, const bool a) { input_data.m_mouse_clicked[int(key)] = a; }
  static void set_mouse_held(const MouseCode key, const bool a) { input_data.m_mouse_held[int(key)] = a; }

  static void cursor_pos_callback(GLFWwindow* window, double xpos_in, double ypos_in);
  static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
};
}
