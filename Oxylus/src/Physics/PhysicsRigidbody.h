#pragma once

namespace Oxylus {
  class PhysicsRigidbody {
  public:
    enum class RigidBodyType {
      Static,
      Dynamic
    };

    PhysicsRigidbody();
    ~PhysicsRigidbody();
  };
}
