#include "src/oxpch.h"
#include "Window.h"

#include "Core/EmbeddedResources.h"
#include "Utils/Log.h"

#include "tinygltf/stb_image.h"
#include "Vulkan/VulkanRenderer.h"

namespace Oxylus {
  Window::WindowData Window::s_WindowData;
  GLFWwindow* Window::s_WindowHandle;

  void Window::InitWindow(const AppSpec& spec) {
    switch (spec.Backend) {
      case Core::RenderBackend::Vulkan: InitVulkanWindow(spec);
        break;
    }
  }

  void Window::InitVulkanWindow(const AppSpec& spec) {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    if (spec.CustomWindowTitle)
      glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    s_WindowHandle = glfwCreateWindow(GetWidth(), GetHeight(), spec.Name.c_str(), nullptr, nullptr);
    glfwSetWindowUserPointer(s_WindowHandle, &s_WindowData);

    //Load file icon
    {
      int width, height, channels;
      const auto imageData = stbi_load_from_memory(EngineLogo, EngineLogoLen, &width, &height, &channels, 4);
      const GLFWimage windowIcon{.width = 40, .height = 40, .pixels = imageData,};
      glfwSetWindowIcon(s_WindowHandle, 1, &windowIcon);
      stbi_image_free(imageData);
    }
    if (s_WindowHandle == nullptr) {
      OX_CORE_FATAL("Failed to create GLFW WindowHandle");
      glfwTerminate();
    }
    glfwSetWindowCloseCallback(s_WindowHandle, CloseWindow);
    glfwSetFramebufferSizeCallback(s_WindowHandle,
      [](GLFWwindow*, int width, int height) {
        s_WindowData.ScreenExtent.width = width;
        s_WindowData.ScreenExtent.height = height;
        OnResize();
      });
    OnResize();
  }

  void Window::OnResize() {
    VulkanRenderer::OnResize();
  }

  void Window::UpdateWindow() {
    glfwPollEvents();
    //glfwWaitEvents();
  }

  void Window::CloseWindow(GLFWwindow*) {
    Application::Get().Close();
    glfwTerminate();
  }

  GLFWwindow* Window::GetGLFWWindow() {
    if (s_WindowHandle == nullptr) {
      OX_CORE_FATAL("Glfw WindowHandle is nullptr. Did you call InitWindow() ?");
    }
    return s_WindowHandle;
  }

  bool Window::IsFocused() {
    return glfwGetWindowAttrib(GetGLFWWindow(), GLFW_FOCUSED);
  }

  bool Window::IsMinimized() {
    return glfwGetWindowAttrib(GetGLFWWindow(), GLFW_ICONIFIED);
  }

  void Window::Minimize() {
    glfwIconifyWindow(s_WindowHandle);
  }

  void Window::Maximize() {
    glfwMaximizeWindow(s_WindowHandle);
  }

  bool Window::IsMaximized() {
    return glfwGetWindowAttrib(s_WindowHandle, GLFW_MAXIMIZED);
  }

  void Window::Restore() {
    glfwRestoreWindow(s_WindowHandle);
  }

  void Window::WaitForEvents() {
    glfwWaitEvents();
  }
}
