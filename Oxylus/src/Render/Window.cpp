#include "Window.h"

#include "Core/EmbeddedLogo.h"
#include "Utils/Log.h"

#include "stb_image.h"

#include "Core/ApplicationEvents.h"

#include "Utils/Profiler.h"

namespace Oxylus {
Window::WindowData Window::s_window_data;
GLFWwindow* Window::s_window_handle;

void Window::init_window(const AppSpec& spec) {
  init_vulkan_window(spec);
}

void Window::init_vulkan_window(const AppSpec& spec) {
  glfwInit();

  constexpr auto window_width = 1600;
  constexpr auto window_height = 900;

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  s_window_handle = glfwCreateWindow(window_width, window_height, spec.name.c_str(), nullptr, nullptr);

  // center window
  const auto center = get_center_pos(window_width, window_height);
  glfwSetWindowPos(s_window_handle, center.x, center.y);

  //Load file icon
  {
    int width, height, channels;
    const auto image_data = stbi_load_from_memory(EngineLogo, (int)EngineLogoLen, &width, &height, &channels, 4);
    const GLFWimage window_icon{.width = 40, .height = 40, .pixels = image_data,};
    glfwSetWindowIcon(s_window_handle, 1, &window_icon);
    stbi_image_free(image_data);
  }
  if (s_window_handle == nullptr) {
    OX_CORE_ERROR("Failed to create GLFW WindowHandle");
    glfwTerminate();
  }

  glfwSetWindowCloseCallback(s_window_handle, close_window);

  glfwSetKeyCallback(
    get_glfw_window(),
    [](GLFWwindow*, const int key, int, const int action, int) {
      switch (action) {
        case GLFW_PRESS: {
          s_window_data.dispatcher->trigger(KeyPressedEvent((KeyCode)key, 0));
          break;
        }
        case GLFW_RELEASE: {
          s_window_data.dispatcher->trigger(KeyReleasedEvent((KeyCode)key));
          break;
        }
        case GLFW_REPEAT: {
          s_window_data.dispatcher->trigger(KeyPressedEvent((KeyCode)key, 1));
          break;
        }
      }
    });

  glfwSetMouseButtonCallback(
    get_glfw_window(),
    [](GLFWwindow*, int button, int action, int) {
      switch (action) {
        case GLFW_PRESS: {
          s_window_data.dispatcher->trigger(MouseButtonPressedEvent((MouseCode)button));
          break;
        }
        case GLFW_RELEASE: {
          s_window_data.dispatcher->trigger(MouseButtonReleasedEvent((MouseCode)button));
          break;
        }
      }
    });
}

void Window::poll_events() {
  OX_SCOPED_ZONE;
  glfwPollEvents();
}

void Window::close_window(GLFWwindow*) {
  Application::get()->close();
  glfwTerminate();
}

void Window::set_window_user_data(void* data) {
  glfwSetWindowUserPointer(get_glfw_window(), data);
}

GLFWwindow* Window::get_glfw_window() {
  if (s_window_handle == nullptr) {
    OX_CORE_ERROR("Glfw WindowHandle is nullptr. Did you call InitWindow() ?");
  }
  return s_window_handle;
}

uint32_t Window::get_width() {
  int width, height;
  glfwGetWindowSize(s_window_handle, &width, &height);
  return (uint32_t)width;
}

uint32_t Window::get_height() {
  int width, height;
  glfwGetWindowSize(s_window_handle, &width, &height);
  return (uint32_t)height;
}

IVec2 Window::get_center_pos(const int width, const int height) {
  int32_t monitor_width = 0, monitor_height = 0;
  int32_t monitor_posx = 0, monitor_posy = 0;
  glfwGetMonitorWorkarea(glfwGetPrimaryMonitor(), &monitor_posx, &monitor_posy, &monitor_width, &monitor_height);
  const auto video_mode = glfwGetVideoMode(glfwGetPrimaryMonitor());

  return {
    monitor_posx + (video_mode->width - width) / 2,
    monitor_posy + (video_mode->height - height) / 2
  };
}

bool Window::is_focused() {
  return glfwGetWindowAttrib(get_glfw_window(), GLFW_FOCUSED);
}

bool Window::is_minimized() {
  return glfwGetWindowAttrib(get_glfw_window(), GLFW_ICONIFIED);
}

void Window::minimize() {
  glfwIconifyWindow(s_window_handle);
}

void Window::maximize() {
  glfwMaximizeWindow(s_window_handle);
}

bool Window::is_maximized() {
  return glfwGetWindowAttrib(s_window_handle, GLFW_MAXIMIZED);
}

void Window::restore() {
  glfwRestoreWindow(s_window_handle);
}

bool Window::is_decorated() {
  return (bool)glfwGetWindowAttrib(s_window_handle, GLFW_DECORATED);
}

void Window::set_undecorated() {
  glfwSetWindowAttrib(s_window_handle, GLFW_DECORATED, false);
}

void Window::set_decorated() {
  glfwSetWindowAttrib(s_window_handle, GLFW_DECORATED, true);
}

bool Window::is_floating() {
  return (bool)glfwGetWindowAttrib(s_window_handle, GLFW_FLOATING);
}

void Window::set_floating() {
  glfwSetWindowAttrib(s_window_handle, GLFW_FLOATING, true);
}

void Window::set_not_floating() {
  glfwSetWindowAttrib(s_window_handle, GLFW_FLOATING, false);
}

bool Window::is_fullscreen_borderless() {
  return s_window_data.is_fullscreen_borderless;
}

void Window::set_fullscreen_borderless() {
  auto* monitor = glfwGetPrimaryMonitor();
  const GLFWvidmode* mode = glfwGetVideoMode(monitor);
  glfwSetWindowMonitor(s_window_handle, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
  s_window_data.is_fullscreen_borderless = true;
}

void Window::set_windowed() {
  auto* monitor = glfwGetPrimaryMonitor();
  const GLFWvidmode* mode = glfwGetVideoMode(monitor);
  const auto center = get_center_pos(1600, 900);
  glfwSetWindowMonitor(s_window_handle, nullptr, center.x, center.y, 1600, 900, mode->refreshRate);
  s_window_data.is_fullscreen_borderless = false;
}

void Window::wait_for_events() {
  OX_SCOPED_ZONE;
  glfwWaitEvents();
}
}
