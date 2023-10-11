#include "Window.h"

#include "Core/EmbeddedLogo.h"
#include "Utils/Log.h"

#include "tinygltf/stb_image.h"
#include "Vulkan/VulkanRenderer.h"

namespace Oxylus {
Window::WindowData Window::s_window_data;
GLFWwindow* Window::s_window_handle;

void Window::init_window(const AppSpec& spec) {
  init_vulkan_window(spec);
}

void Window::init_vulkan_window(const AppSpec& spec) {
  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  if (spec.custom_window_title)
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
  s_window_handle = glfwCreateWindow(1600, 900, spec.name.c_str(), nullptr, nullptr);
  glfwSetWindowUserPointer(s_window_handle, &s_window_data);

  //Load file icon
  {
    int width, height, channels;
    const auto imageData = stbi_load_from_memory(EngineLogo, (int)EngineLogoLen, &width, &height, &channels, 4);
    const GLFWimage windowIcon{.width = 40, .height = 40, .pixels = imageData,};
    glfwSetWindowIcon(s_window_handle, 1, &windowIcon);
    stbi_image_free(imageData);
  }
  if (s_window_handle == nullptr) {
    OX_CORE_FATAL("Failed to create GLFW WindowHandle");
    glfwTerminate();
  }
  glfwSetWindowCloseCallback(s_window_handle, close_window);
}

void Window::update_window() {
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
    OX_CORE_FATAL("Glfw WindowHandle is nullptr. Did you call InitWindow() ?");
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

void Window::wait_for_events() {
  glfwWaitEvents();
}
}
