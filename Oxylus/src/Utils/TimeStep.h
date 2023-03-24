#pragma once

#include <glfw/glfw3.h>

namespace Oxylus {
  class Timestep {
  public:
    static float GetDeltaTime() {
      return deltaTime;
    }

    static void UpdateTime() {
      float currentFrame = static_cast<float>(glfwGetTime());
      deltaTime = currentFrame - lastFrame;
      lastFrame = currentFrame;
    }

  private:
    inline static float deltaTime = 0.0f;
    inline static float lastFrame = 0.0f;
  };
}
