#pragma once

namespace Oxylus {
  class Physics {
  public:
    static void InitPhysics();
    static void Update(float deltaTime);
    static void CleanupPhysics();
  };
}
