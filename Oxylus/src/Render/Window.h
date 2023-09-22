#pragma once
#include "Core/Input.h"
#include "Core/Application.h"

namespace Oxylus {
class Window {
public:
  static void InitWindow(const AppSpec& spec);
  static void UpdateWindow();
  static void CloseWindow(GLFWwindow* window);
  static GLFWwindow* GetGLFWWindow();

  static uint32_t GetWidth() { return s_WindowData.ScreenExtent.width; }
  static uint32_t GetHeight() { return s_WindowData.ScreenExtent.height; }
  static VkExtent2D& GetWindowExtent() { return s_WindowData.ScreenExtent; }

  static bool IsFocused();
  static bool IsMinimized();
  static void Minimize();
  static void Maximize();
  static bool IsMaximized();
  static void Restore();

  static void WaitForEvents();

private:
  static struct WindowData {
    VkExtent2D ScreenExtent = {1600, 900};
    bool IsOverTitleBar = false;
  } s_WindowData;

  static void InitVulkanWindow(const AppSpec& spec);
  static GLFWwindow* s_WindowHandle;
};
}
