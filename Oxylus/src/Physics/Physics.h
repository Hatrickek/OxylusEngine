#pragma once
#include "PxPhysicsAPI.h"

namespace Oxylus {
  class Physics {
  public:
    static void InitPhysics();
    static void Update(float deltaTime);
    static void CleanupPhysics();

    static physx::PxPhysics* s_Physics;
    static physx::PxScene* s_Scene;
    static float yGravity;
  };
}
