#pragma once
#include "Core/Input.h"
#include "Core/Application.h"
#include "Core/Types.h"

namespace Oxylus {
class Window {
public:
  static void init_window(const AppSpec& spec);
  static void update_window();
  static void close_window(GLFWwindow* window);

  static void set_window_user_data(void* data);

  static GLFWwindow* get_glfw_window();
  static uint32_t get_width();
  static uint32_t get_height();

  static bool is_focused();
  static bool is_minimized();
  static void minimize();
  static void maximize();
  static bool is_maximized();
  static void restore();

  static void wait_for_events();

private:
  static struct WindowData {
    bool is_over_title_bar = false;
  } s_window_data;

  static void init_vulkan_window(const AppSpec& spec);
  static GLFWwindow* s_window_handle;
};
}
